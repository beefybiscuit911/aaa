#include "screen.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "includes.h"
#include "logger.hpp"
#include <opencv2/opencv.hpp>

#ifdef _DEBUG
#pragma comment(lib, "opencv_world4120d.lib")
#else
#pragma comment(lib, "opencv_world4120.lib")
#endif

// Static member definitions
static cv::VideoCapture g_cameraCap;
std::mutex CameraCapture::captureMutex;

// Global variable definitions

ColorUtils::HSV ColorUtils::HSV::fromRGB(int r, int g, int b) {
  HSV hsv;
  const float r_f = r / 255.0f;
  const float g_f = g / 255.0f;
  const float b_f = b / 255.0f;

  const float minVal = (std::min)({r_f, g_f, b_f});
  const float maxVal = (std::max)({r_f, g_f, b_f});

  const float delta = maxVal - minVal;

  hsv.v = maxVal;
  if (delta < 0.00001f)
    return {0.0f, 0.0f, maxVal};

  hsv.s = delta / maxVal;
  if (r_f >= maxVal)
    hsv.h = (g_f - b_f) / delta;
  else if (g_f >= maxVal)
    hsv.h = 2.0f + (b_f - r_f) / delta;
  else
    hsv.h = 4.0f + (r_f - g_f) / delta;

  hsv.h *= 60.0f;
  if (hsv.h < 0.0f)
    hsv.h += 360.0f;
  return hsv;
}

namespace MemorySafe {
template <typename T> void secureRelease(T *ptr, size_t size) {
  if (ptr) {
    SecureZeroMemory(ptr, size);
    delete[] ptr;
  }
}
template void secureRelease<unsigned int>(unsigned int *, size_t);
} // namespace MemorySafe

unsigned int *CameraCapture::captureFromOBSCamera(int width, int height) {
  static bool initialized = false;
  // Note: g_cameraCap is defined at the top of the file

  if (!initialized) {
    // Try direct device index first (more reliable)
    if (!g_cameraCap.open(0, cv::CAP_DSHOW)) {
      return nullptr;
    }
    g_cameraCap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    g_cameraCap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    g_cameraCap.set(cv::CAP_PROP_FPS, 144);
    g_cameraCap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    initialized = true;
  }

  cv::Mat frame;
  if (!g_cameraCap.read(frame) || frame.empty())
    return nullptr;

  // Direct conversion to BGRA
  cv::Mat bgraFrame;
  cv::cvtColor(frame, bgraFrame, cv::COLOR_BGR2BGRA);

  auto *pixels = new unsigned int[width * height];
  memcpy(pixels, bgraFrame.data, width * height * 4);
  return pixels;
}

unsigned int *captureFrame(int width, int height, int offsetX, int offsetY) {
  // Try OBS camera first (only if capturing full screen/center)
  if (offsetX == 0 && offsetY == 0) {
    if (auto pixels = CameraCapture::captureFromOBSCamera(width, height)) {
      return pixels;
    }
  }

  // Screen capture - capture specific region
  HDC hdcScreen = GetDC(nullptr);
  HDC hdcMem = CreateCompatibleDC(hdcScreen);
  HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
  HGDIOBJ oldObj = SelectObject(hdcMem, hBitmap);

  // Capture specific region of screen (or center if offsets are 0)
  int screenX = (offsetX == 0 && offsetY == 0)
                    ? (GetSystemMetrics(SM_CXSCREEN) - width) / 2
                    : offsetX;
  int screenY = (offsetX == 0 && offsetY == 0)
                    ? (GetSystemMetrics(SM_CYSCREEN) - height) / 2
                    : offsetY;

  BitBlt(hdcMem, 0, 0, width, height, hdcScreen, screenX, screenY, SRCCOPY);

  // Get pixels
  BITMAPINFOHEADER bmi = {0};
  bmi.biSize = sizeof(BITMAPINFOHEADER);
  bmi.biWidth = width;
  bmi.biHeight = -height; // Top-down
  bmi.biPlanes = 1;
  bmi.biBitCount = 32;
  bmi.biCompression = BI_RGB;

  unsigned int *pixels = new unsigned int[width * height];
  GetDIBits(hdcMem, hBitmap, 0, height, pixels, (BITMAPINFO *)&bmi,
            DIB_RGB_COLORS);

  // Cleanup
  SelectObject(hdcMem, oldObj);
  DeleteObject(hBitmap);
  DeleteDC(hdcMem);
  ReleaseDC(nullptr, hdcScreen);

  return pixels;
}
bool analyzePixels(unsigned int *pixels, int width, int height, int &targetX,
                   int &targetY, int &targetHeight, int &minX, int &minY,
                   int &maxX, int &maxY) {
  // Static variables for bounding box smoothing to prevent jitter
  static float smoothedMinX = 0.0f, smoothedMinY = 0.0f;
  static float smoothedMaxX = 0.0f, smoothedMaxY = 0.0f;
  static bool hasPreviousBox = false;
  const float smoothingFactor = 0.7f; // Higher = more smoothing (less jitter)

  // Get target color from config (not static, so it updates if config changes)
  ColorUtils::Color targetColor(config.targetColor.c_str());
  const int targetR = targetColor.r;
  const int targetG = targetColor.g;
  const int targetB = targetColor.b;
  const int tolerance = config.tolerance;

  // Pixel format from GetDIBits with DIB_RGB_COLORS is BGRA (Blue, Green, Red,
  // Alpha)
  auto checkPixel = [&](unsigned int pixel) -> bool {
    // Extract BGRA components (GetDIBits returns BGRA format)
    int b = (pixel) & 0xFF;       // Blue channel
    int g = (pixel >> 8) & 0xFF;  // Green channel
    int r = (pixel >> 16) & 0xFF; // Red channel

    // Fast Manhattan distance check
    int dr = (r > targetR) ? (r - targetR) : (targetR - r);
    int dg = (g > targetG) ? (g - targetG) : (targetG - g);
    int db = (b > targetB) ? (b - targetB) : (targetB - b);

    return (dr + dg + db) < tolerance;
  };

  // FULL SCAN: Scan entire image to find ALL target pixels
  // This ensures we capture the complete target, not just 70%
  // Use step=2 for good coverage while maintaining performance
  std::vector<cv::Point> targetPoints;
  std::vector<int> yValues;
  targetPoints.reserve(500); // Reserve more memory for full scan
  yValues.reserve(500);

  // Scan entire image with step=2 for good coverage and performance
  // This captures ~4x more pixels than the old step=3 spiral search
  const int scanStep = 2;
  for (int y = 0; y < height; y += scanStep) {
    for (int x = 0; x < width; x += scanStep) {
      unsigned int pixel = pixels[y * width + x];
      if (checkPixel(pixel)) {
        targetPoints.emplace_back(x, y);
        yValues.push_back(y);
      }
    }
  }

  // If we found a target, do a second pass to expand the bounding box
  // This ensures we capture edge pixels that might have been skipped
  if (!targetPoints.empty()) {
    int tempMinX = width, tempMaxX = 0;
    int tempMinY = height, tempMaxY = 0;
    for (const auto &pt : targetPoints) {
      if (pt.x < tempMinX)
        tempMinX = pt.x;
      if (pt.x > tempMaxX)
        tempMaxX = pt.x;
      if (pt.y < tempMinY)
        tempMinY = pt.y;
      if (pt.y > tempMaxY)
        tempMaxY = pt.y;
    }

    // Expand search area slightly to catch edge pixels
    int expandX = scanStep * 2;
    int expandY = scanStep * 2;
    int searchMinX = std::max(0, tempMinX - expandX);
    int searchMaxX = std::min(width - 1, tempMaxX + expandX);
    int searchMinY = std::max(0, tempMinY - expandY);
    int searchMaxY = std::min(height - 1, tempMaxY + expandY);

    // Scan the expanded area with step=1 for precision
    for (int y = searchMinY; y <= searchMaxY; y++) {
      for (int x = searchMinX; x <= searchMaxX; x++) {
        unsigned int pixel = pixels[y * width + x];
        if (checkPixel(pixel)) {
          targetPoints.emplace_back(x, y);
          yValues.push_back(y);
        }
      }
    }
  }

  if (targetPoints.empty()) {
    hasPreviousBox = false; // Reset if no target found
    return false;
  }

  // Calculate raw bounding box from all detected points
  int rawMinX = width, rawMaxX = 0;
  int rawMinY = height, rawMaxY = 0;

  for (const auto &point : targetPoints) {
    if (point.x < rawMinX)
      rawMinX = point.x;
    if (point.x > rawMaxX)
      rawMaxX = point.x;
    if (point.y < rawMinY)
      rawMinY = point.y;
    if (point.y > rawMaxY)
      rawMaxY = point.y;
  }

  // Expand bounding box slightly to ensure full coverage (like ESP boxes)
  // Add 2-3 pixel padding on all sides
  const int padding = 3;
  rawMinX = std::max(0, rawMinX - padding);
  rawMinY = std::max(0, rawMinY - padding);
  rawMaxX = std::min(width - 1, rawMaxX + padding);
  rawMaxY = std::min(height - 1, rawMaxY + padding);

  targetHeight = rawMaxY - rawMinY;
  if (targetHeight < 5) {
    hasPreviousBox = false;
    return false;
  }

  // Apply smoothing to bounding box to prevent jitter
  if (hasPreviousBox) {
    // Smooth the bounding box coordinates
    smoothedMinX =
        smoothedMinX * (1.0f - smoothingFactor) + rawMinX * smoothingFactor;
    smoothedMinY =
        smoothedMinY * (1.0f - smoothingFactor) + rawMinY * smoothingFactor;
    smoothedMaxX =
        smoothedMaxX * (1.0f - smoothingFactor) + rawMaxX * smoothingFactor;
    smoothedMaxY =
        smoothedMaxY * (1.0f - smoothingFactor) + rawMaxY * smoothingFactor;
  } else {
    // First frame - initialize with raw values
    smoothedMinX = static_cast<float>(rawMinX);
    smoothedMinY = static_cast<float>(rawMinY);
    smoothedMaxX = static_cast<float>(rawMaxX);
    smoothedMaxY = static_cast<float>(rawMaxY);
    hasPreviousBox = true;
  }

  // Convert smoothed values back to integers
  minX = static_cast<int>(smoothedMinX + 0.5f);
  minY = static_cast<int>(smoothedMinY + 0.5f);
  maxX = static_cast<int>(smoothedMaxX + 0.5f);
  maxY = static_cast<int>(smoothedMaxY + 0.5f);

  // Ensure valid bounds
  minX = std::max(0, std::min(width - 1, minX));
  minY = std::max(0, std::min(height - 1, minY));
  maxX = std::max(0, std::min(width - 1, maxX));
  maxY = std::max(0, std::min(height - 1, maxY));

  // Recalculate targetHeight from smoothed box
  targetHeight = maxY - minY;
  if (targetHeight < 5)
    return false;

  // Calculate target position with headshot adjustment
  // Use config.headshotPriority to blend between center and headshot position
  targetX = (minX + maxX) / 2;

  // Calculate body center position
  int bodyCenterY = (minY + maxY) / 2;

  // Calculate headshot position (upper portion of bounding box)
  std::sort(yValues.begin(), yValues.end());
  int headY = yValues[yValues.size() / 3];
  int headshotY = headY - (targetHeight / 10);

  // Apply headshot adjustment factor
  // Apply headshot adjustment factor (Legacy support using Y-offset)
  float headOffsetY = std::abs(config.headOffsetY);
  float adj =
      std::max(1.0f, std::min(5.0f, (headOffsetY > 0.1f) ? headOffsetY : 1.0f));
  int adjustedHeadshotY = minY + static_cast<int>((headshotY - minY) / adj);

  // Blend between body center and headshot based on headshotPriority
  // headshotPriority = 0.0 -> body center, 1.0 -> full headshot
  float priority =
      std::max(0.0f, std::min(1.0f, (float)config.headshotPriority));
  targetY = static_cast<int>(bodyCenterY * (1.0f - priority) +
                             adjustedHeadshotY * priority);

  return true;
}