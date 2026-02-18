#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Forward declarations for TensorRT types (to avoid including heavy headers in
// .hpp)
namespace nvinfer1 {
class ICudaEngine;
class IExecutionContext;
class IRuntime;
} // namespace nvinfer1

struct Detection {
  float x, y, w, h; // Bounding box (center x, y, width, height)
  float confidence; // Detection confidence
  int classId;      // 0 = head, 1 = body, etc.
};

class TensorRTInference {
public:
  TensorRTInference();
  ~TensorRTInference();

  bool LoadModel(const std::string &modelPath);
  void SetInputSize(int size) { m_inputSize = size; }
  bool RunInference(const std::vector<uint8_t> &imageData, int width,
                    int height, std::vector<Detection> &outDetections);
  float GetLastInferenceTime() const { return m_lastInferenceMs; }

private:
  bool m_loaded;
  float m_lastInferenceMs;

  bool BuildEngineFromOnnx(const std::string &onnxPath,
                           const std::string &enginePath);

  // TensorRT objects
  nvinfer1::IRuntime *m_runtime;
  nvinfer1::ICudaEngine *m_engine;
  nvinfer1::IExecutionContext *m_context;
  void *m_stream; // cudaStream_t

  // GPU buffers
  void *m_inputBuffer;
  void *m_outputBuffer;

  // Host buffers (Turbo Buffers: Pre-allocated to avoid heap churn)
  std::vector<float> m_inputHost;
  std::vector<float> m_outputHost;

  // Model info
  int m_inputSize;  // Usually 640 or 320
  int m_outputSize; // Depends on model
};
