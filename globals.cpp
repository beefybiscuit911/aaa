#include "globals.hpp"
#include "config.hpp"
#include "hid_link.hpp"

// Global instances
HidLink *gHidLink = nullptr;

// Aiming state
volatile bool isAimingActive = false;
int gCaptureOffsetX = 0;
int gCaptureOffsetY = 0;

// Flickbot state
bool isFlickbotActive = false;
bool flickbotHasShot = false;

// Mouse button state
bool gMB1P = false;
std::chrono::steady_clock::time_point gMB1ST = std::chrono::steady_clock::now();

// Web UI stats
std::atomic<int> g_currentFPS{0};
std::atomic<float> g_gpuMemoryPercent{0.0f};
std::atomic<float> g_lastInferenceMs{0.0f};
std::atomic<bool> g_tensorrtLoaded{false};
std::atomic<bool> g_cudaAvailable{false};
std::atomic<bool> g_captureActive{false};
std::string g_activeCaptureEngine = "None";

// AI Debug Preview
std::vector<uint8_t> g_debugFrame;
std::mutex g_debugFrameMutex;
std::atomic<bool> g_newFrameAvailable{false};

// Motion Bridging (144Hz Interpolation)
std::atomic<float> g_targetVelX{0.0f};
std::atomic<float> g_targetVelY{0.0f};
std::atomic<float> g_lastTargetX{0.0f};
std::atomic<float> g_lastTargetY{0.0f};
std::atomic<float> g_smoothedTargetX{0.0f};
std::atomic<float> g_smoothedTargetY{0.0f};
std::chrono::steady_clock::time_point g_lastUpdateStartTime =
    std::chrono::steady_clock::now();
