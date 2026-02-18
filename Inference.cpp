#include "Inference.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>

// TensorRT headers
#include "config.hpp"
#include <NvInfer.h>
#include <NvInferRuntime.h>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>

using namespace nvinfer1;

// Logger for TensorRT
class Logger : public ILogger {
  void log(Severity severity, const char *msg) noexcept override {
    if (severity <= Severity::kWARNING)
      std::cout << "[TensorRT] " << msg << std::endl;
  }
} gLogger;

TensorRTInference::TensorRTInference()
    : m_loaded(false), m_lastInferenceMs(0.0f), m_runtime(nullptr),
      m_engine(nullptr), m_context(nullptr), m_stream(nullptr),
      m_inputBuffer(nullptr), m_outputBuffer(nullptr), m_inputSize(640),
      m_outputSize(0) {}

TensorRTInference::~TensorRTInference() {
  if (m_inputBuffer)
    cudaFree(m_inputBuffer);
  if (m_outputBuffer)
    cudaFree(m_outputBuffer);
  if (m_stream)
    cudaStreamDestroy((cudaStream_t)m_stream);
  if (m_context)
    delete m_context;
  if (m_engine)
    delete m_engine;
  if (m_runtime)
    delete m_runtime;
}

bool TensorRTInference::LoadModel(const std::string &modelPath) {
  std::cout << "[TensorRT] Loading model: " << modelPath << std::endl;

  // Create runtime
  m_runtime = createInferRuntime(gLogger);
  if (!m_runtime) {
    std::cerr << "[TensorRT] Failed to create runtime" << std::endl;
    return false;
  }

  auto attemptLoad = [&](const std::string &path) -> bool {
    std::ifstream file(path, std::ios::binary);
    if (!file.good())
      return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> engineData(size);
    file.read(engineData.data(), size);
    file.close();

    m_engine = m_runtime->deserializeCudaEngine(engineData.data(), size);
    return m_engine != nullptr;
  };

  // Try to load engine
  if (!attemptLoad(modelPath)) {
    std::cout << "[TensorRT] Engine load failed (Incompatible version?). "
                 "Attempting Auto-Healing..."
              << std::endl;

    // Check for ONNX alternative
    std::string onnxPath = modelPath;
    size_t lastDot = onnxPath.find_last_of(".");
    if (lastDot != std::string::npos) {
      onnxPath = onnxPath.substr(0, lastDot) + ".onnx";
    }

    std::ifstream onnxFile(onnxPath);
    if (onnxFile.good()) {
      onnxFile.close();
      std::cout << "[TensorRT] Found " << onnxPath
                << ", rebuilding engine for this hardware..." << std::endl;
      if (BuildEngineFromOnnx(onnxPath, modelPath)) {
        std::cout << "[TensorRT] SUCCESS: Engine rebuilt. Retrying load..."
                  << std::endl;
        if (!attemptLoad(modelPath)) {
          std::cerr
              << "[TensorRT] CRITICAL: Rebuilt engine still fails to load!"
              << std::endl;
          return false;
        }
      } else {
        std::cerr << "[TensorRT] ERROR: Failed to rebuild engine from ONNX."
                  << std::endl;
        return false;
      }
    } else {
      std::cerr << "[TensorRT] ERROR: No " << onnxPath
                << " found to rebuild from." << std::endl;
      return false;
    }
  }

  // Create execution context
  m_context = m_engine->createExecutionContext();
  if (!m_context) {
    std::cerr << "[TensorRT] Failed to create execution context" << std::endl;
    return false;
  }

  // Create CUDA stream
  cudaStream_t stream;
  cudaStreamCreate(&stream);
  // 4. Query engine for actual input resolution (Auto-Sensing)
  auto dims = m_engine->getTensorShape("images");
  if (dims.nbDims >= 4) {
    m_inputSize =
        (int)dims.d[2]; // Usually [1, 3, 640, 640] or [1, 3, 320, 320]
  } else {
    m_inputSize = 640; // Fallback
  }
  m_stream = stream;

  // Calculate number of detections based on input size
  // YOLOv8 has 3 scales: /8, /16, /32
  int s1 = m_inputSize / 8;
  int s2 = m_inputSize / 16;
  int s3 = m_inputSize / 32;
  int numDetections = (s1 * s1) + (s2 * s2) + (s3 * s3);

  // Allocate GPU buffers
  size_t inputBytes = 1 * 3 * m_inputSize * m_inputSize * sizeof(float);
  cudaMalloc(&m_inputBuffer, inputBytes);

  // YOLOv8 output size: [1, num_classes + 4, num_detections]
  // We assume 80 classes + 4 bbox coords = 84 channels
  m_outputSize = 84 * numDetections;
  size_t outputBytes = m_outputSize * sizeof(float);
  cudaMalloc(&m_outputBuffer, outputBytes);

  // Pre-allocate Turbo Buffers (Host side)
  m_inputHost.resize(3 * m_inputSize * m_inputSize, 0.0f);
  m_outputHost.resize(m_outputSize, 0.0f);

  m_loaded = true;
  std::cout << "[TensorRT] SUCCESS: Model loaded." << std::endl;
  std::cout << "[TensorRT] Detected Engine Resolution: " << m_inputSize << "x"
            << m_inputSize << std::endl;

  if (m_inputSize > 320) {
    std::cout << "[TensorRT] TIP: You are using a " << m_inputSize
              << " engine. For 2x more speed, export your model to 320x320!"
              << std::endl;
  } else {
    std::cout << "[TensorRT] TURBO: 320x320 engine detected. Maximum "
                 "performance enabled."
              << std::endl;
  }

  return true;
}

bool TensorRTInference::RunInference(const std::vector<uint8_t> &imageData,
                                     int width, int height,
                                     std::vector<Detection> &outDetections) {
  if (!m_loaded)
    return false;

  auto start = std::chrono::high_resolution_clock::now();

  // 1. Preprocess: Use pre-allocated m_inputHost (Zero heap churn)
  float *rPtr = m_inputHost.data();
  float *gPtr = rPtr + m_inputSize * m_inputSize;
  float *bPtr = gPtr + m_inputSize * m_inputSize;

  const float inv255 = 1.0f / 255.0f;
  const uint8_t *srcData = imageData.data();

  // Adaptive Preprocessing: Auto-resize if capture != model resolution
  bool needsResize = (width != m_inputSize || height != m_inputSize);

  for (int y = 0; y < m_inputSize; y++) {
    int srcY = needsResize ? (y * height / m_inputSize) : y;
    const uint8_t *rowStart = srcData + (srcY * width * 4);
    for (int x = 0; x < m_inputSize; x++) {
      int srcX = needsResize ? (x * width / m_inputSize) : x;
      const uint8_t *pixel = rowStart + (srcX * 4);

      int dstIdx = y * m_inputSize + x;
      rPtr[dstIdx] = pixel[2] * inv255;
      gPtr[dstIdx] = pixel[1] * inv255;
      bPtr[dstIdx] = pixel[0] * inv255;
    }
  }

  // 2. Copy input to GPU
  cudaMemcpyAsync(m_inputBuffer, m_inputHost.data(),
                  (3 * m_inputSize * m_inputSize) * sizeof(float),
                  cudaMemcpyHostToDevice, (cudaStream_t)m_stream);

  // 3. Run inference
  m_context->setTensorAddress("images", m_inputBuffer);
  m_context->setTensorAddress("output0", m_outputBuffer);
  m_context->enqueueV3((cudaStream_t)m_stream);

  // 4. Copy output from GPU into m_outputHost (Turbo)
  cudaMemcpyAsync(m_outputHost.data(), m_outputBuffer,
                  m_outputSize * sizeof(float), cudaMemcpyDeviceToHost,
                  (cudaStream_t)m_stream);
  cudaStreamSynchronize((cudaStream_t)m_stream);

  // 5. Post-process: Parse YOLOv8 output
  float *outputTensor = m_outputHost.data();
  outDetections.clear();

  int s1 = m_inputSize / 8;
  int s2 = m_inputSize / 16;
  int s3 = m_inputSize / 32;
  int numDetections = (s1 * s1) + (s2 * s2) + (s3 * s3);

  // Coordinate Mapping: Scale model detections (0..m_inputSize) back to capture
  // dimensions (0..width)
  float coordScale = (float)width / (float)m_inputSize;
  int numClasses = 80;

  for (int i = 0; i < numDetections; i++) {
    // Get bbox (x, y, w, h)
    float x = outputTensor[i];
    float y = outputTensor[numDetections + i];
    float w = outputTensor[2 * numDetections + i];
    float h = outputTensor[3 * numDetections + i];

    // Get class scores (skip first 4 bbox values)
    float maxConf = 0.0f;
    int maxClass = -1;

    for (int c = 0; c < 2; c++) { // Only check head/body classes
      float conf = outputTensor[(4 + c) * numDetections + i];
      if (conf > maxConf) {
        maxConf = conf;
        maxClass = c;
      }
    }

    // Filter by confidence threshold
    if (maxConf > config.confidenceThreshold) {
      Detection det;
      det.x = outputTensor[i] * coordScale;
      det.y = outputTensor[numDetections + i] * coordScale;
      det.w = outputTensor[2 * numDetections + i] * coordScale;
      det.h = outputTensor[3 * numDetections + i] * coordScale;
      det.confidence = maxConf;
      det.classId = maxClass;
      outDetections.push_back(det);
    }
  }

  // 6. Apply NMS (Non-Maximum Suppression)
  // TODO: Implement proper NMS if needed

  auto end = std::chrono::high_resolution_clock::now();
  m_lastInferenceMs =
      std::chrono::duration<float, std::milli>(end - start).count();

  return true;
}

using std::unique_ptr;

bool TensorRTInference::BuildEngineFromOnnx(const std::string &onnxPath,
                                            const std::string &enginePath) {
  std::cout << "[TensorRT] Building engine from: " << onnxPath
            << " (This may take a minute...)" << std::endl;

  auto builder = unique_ptr<IBuilder>(createInferBuilder(gLogger));
  auto config = unique_ptr<IBuilderConfig>(builder->createBuilderConfig());
  auto network = unique_ptr<INetworkDefinition>(builder->createNetworkV2(
      1U << static_cast<uint32_t>(
          NetworkDefinitionCreationFlag::kEXPLICIT_BATCH)));
  auto parser = unique_ptr<nvonnxparser::IParser>(
      nvonnxparser::createParser(*network, gLogger));

  if (!parser->parseFromFile(onnxPath.c_str(),
                             static_cast<int>(ILogger::Severity::kWARNING))) {
    std::cerr << "[TensorRT] ERROR: Failed to parse ONNX file" << std::endl;
    return false;
  }

  // Configuration
  config->setMemoryPoolLimit(MemoryPoolType::kWORKSPACE,
                             1ULL << 30); // 1GB workspace

  // Use FP16 if supported for massive speed boost
  if (builder->platformHasFastFp16()) {
    std::cout << "[TensorRT] Enabling FP16 precision for faster inference."
              << std::endl;
    config->setFlag(BuilderFlag::kFP16);
  }

  // Build the engine
  auto plan = unique_ptr<IHostMemory>(
      builder->buildSerializedNetwork(*network, *config));
  if (!plan) {
    std::cerr << "[TensorRT] ERROR: Failed to build engine plan" << std::endl;
    return false;
  }

  // Save to disk
  std::ofstream outfile(enginePath, std::ios::binary);
  if (!outfile) {
    std::cerr << "[TensorRT] ERROR: Could not open " << enginePath
              << " for writing" << std::endl;
    return false;
  }
  outfile.write(reinterpret_cast<const char *>(plan->data()), plan->size());
  outfile.close();

  return true;
}
