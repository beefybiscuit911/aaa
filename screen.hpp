#pragma once
#include "globals.hpp"
#include "includes.h"
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
class SilentLogger; 

// Add these macros at the top of screen.hpp
#define GetBlue(rgb) ((rgb) & 0xFF)
#define GetGreen(rgb) (((rgb) >> 8) & 0xFF)
#define GetRed(rgb) (((rgb) >> 16) & 0xFF)

namespace ColorUtils {
inline void extractRGB(unsigned int pixel, int &r, int &g, int &b) {
  r = (pixel >> 16) & 0xFF;
  g = (pixel >> 8) & 0xFF;
  b = pixel & 0xFF;
}

struct Color {
  int r, g, b;
  Color() : r(0), g(0), b(0) {}
  Color(const std::string &colorName) {
    if (colorName == "purple") {
      r = 128;
      g = 0;
      b = 128;
    } else if (colorName == "red") {
      r = 255;
      g = 0;
      b = 0;
    } else if (colorName == "green") {
      r = 0;
      g = 255;
      b = 0;
    } else if (colorName == "blue") {
      r = 0;
      g = 0;
      b = 255;
    } else if (colorName == "yellow") {
      r = 255;
      g = 255;
      b = 0;
    } else {
      // Default to purple
      r = 128;
      g = 0;
      b = 128;
    }
  }
};

struct HSV {
  float h, s, v;
  static HSV fromRGB(int r, int g, int b);
};
} // namespace ColorUtils

namespace MemorySafe {
template <typename T> void secureRelease(T *ptr, size_t size);
}

class CameraCapture {
  static std::mutex captureMutex;

public:
  static unsigned int *captureFromOBSCamera(int width, int height);
};

unsigned int *captureFrame(int width, int height, int offsetX = 0,
                           int offsetY = 0);
bool analyzePixels(unsigned int *pixels, int width, int height, int &targetX,
                   int &targetY, int &targetHeight, int &minX, int &minY,
                   int &maxX, int &maxY);