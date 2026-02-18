#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
class HidLink;

// Global instances
extern HidLink *gHidLink;

// Aiming state
extern volatile bool isAimingActive;
extern int gCaptureOffsetX;
extern int gCaptureOffsetY;

// Flickbot state
extern bool isFlickbotActive;
extern bool flickbotHasShot;

// Mouse button state (for RCS)
extern bool gMB1P;                                   // Mouse Button 1 Pressed
extern std::chrono::steady_clock::time_point gMB1ST; // MB1 Start Time

// Web UI stats (updated by main threads)
extern std::atomic<int> g_currentFPS;
extern std::atomic<float> g_gpuMemoryPercent;
extern std::atomic<float> g_lastInferenceMs;
extern std::atomic<bool> g_tensorrtLoaded;
extern std::atomic<bool> g_cudaAvailable;
extern std::atomic<bool> g_captureActive;
extern std::string g_activeCaptureEngine;

// AI Debug Preview
extern std::vector<uint8_t> g_debugFrame;
extern std::mutex g_debugFrameMutex;
extern std::atomic<bool> g_newFrameAvailable;

// Motion Bridging (144Hz Interpolation)
extern std::atomic<float> g_targetVelX;
extern std::atomic<float> g_targetVelY;
extern std::atomic<float> g_lastTargetX;
extern std::atomic<float> g_lastTargetY;
extern std::atomic<float> g_smoothedTargetX;
extern std::atomic<float> g_smoothedTargetY;
extern std::chrono::steady_clock::time_point g_lastUpdateStartTime;
