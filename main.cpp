#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Capture.hpp"
#include "Inference.hpp"
#include "aiming.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "hid_link.hpp"
#include "logger.hpp"
#include "screen.hpp"
// #include "webserver.hpp" // Removed
#include "Config.h" // Added for unload flag
#include "gui.hpp"  // Added
#include <shellapi.h>

const int CAPTURE_W = 320;
const int CAPTURE_H = 320;
const float CENTER_X = 160.0f;
const float CENTER_Y = 160.0f;

// Global detection results (shared between threads)
std::vector<Detection> g_detections;
std::mutex g_detectionsMutex;

// Log buffer for Web UI
std::vector<std::string> g_webLogs;
std::mutex g_logMutex;

void AddWebLog(const std::string &msg) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  g_webLogs.push_back(msg);
  if (g_webLogs.size() > 50)
    g_webLogs.erase(g_webLogs.begin());
}

// Helper to draw a box on a BGRA buffer
void DrawBox(std::vector<uint8_t> &data, int width, int height,
             const Detection &det) {
  int x1 = (int)(det.x - det.w / 2.0f);
  int y1 = (int)(det.y - det.h / 2.0f);
  int x2 = (int)(det.x + det.w / 2.0f);
  int y2 = (int)(det.y + det.h / 2.0f);

  x1 = (std::max)(0, (std::min)(x1, width - 1));
  y1 = (std::max)(0, (std::min)(y1, height - 1));
  x2 = (std::max)(0, (std::min)(x2, width - 1));
  y2 = (std::max)(0, (std::min)(y2, height - 1));

  // Class 1 = Head (Green), Class 0 = Body (Red)
  uint8_t r = (det.classId == 1) ? 0 : 255;
  uint8_t g = (det.classId == 1) ? 255 : 0;
  uint8_t b = 0;

  auto plot = [&](int x, int y) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
      int idx = (y * width + x) * 4;
      data[idx] = b;
      data[idx + 1] = g;
      data[idx + 2] = r;
    }
  };

  // Draw 2-pixel thick box
  for (int t = 0; t < 2; t++) {
    for (int x = x1; x <= x2; x++) {
      plot(x, y1 + t);
      plot(x, y2 - t);
    }
    for (int y = y1; y <= y2; y++) {
      plot(x1 + t, y);
      plot(x2 - t, y);
    }
  }
}

// Helper to draw the adaptive head hitbox (Cyan circle)
void DrawHitbox(std::vector<uint8_t> &data, int width, int height,
                const Detection &det) {
  float headX = det.x;
  float headY = det.y;
  float targetH = det.h;

  if (det.classId == 1) { // Head (Class 1)
    headX = det.x + config.headOffsetX;
    headY = det.y + config.headOffsetY;
  } else { // Body (Class 0)
    headX = det.x + config.bodyOffsetX;
    headY -= (targetH * 0.45f - config.bodyOffsetY);
  }

  float radius = targetH * config.headHitboxScale;

  // Draw Cyan outline (B:255, G:255, R:0)
  for (int y = (int)-radius; y <= (int)radius; y++) {
    for (int x = (int)-radius; x <= (int)radius; x++) {
      float d = std::sqrt((float)(x * x + y * y));
      if (std::abs(d - radius) < 1.0f) {
        int px = (int)(headX + x);
        int py = (int)(headY + y);
        if (px >= 0 && px < width && py >= 0 && py < height) {
          int idx = (py * width + px) * 4;
          data[idx] = 255;     // B
          data[idx + 1] = 255; // G
          data[idx + 2] = 0;   // R
        }
      }
    }
  }
}

// Helper to save a raw BGRA buffer as BMP
void SaveBMP(const std::string &filename, std::vector<uint8_t> data, int width,
             int height, const std::vector<Detection> &detections) {
  if (data.empty())
    return;

  // Draw boxes and hitboxes on the copy (Only high confidence targets)
  for (const auto &det : detections) {
    if (det.confidence < config.confidenceThreshold)
      continue;

    DrawBox(data, width, height, det);
    if (config.flickbotEnabled) {
      DrawHitbox(data, width, height, det);
    }
  }

  std::ofstream f(filename, std::ios::binary);
  if (!f)
    return;

  uint32_t fileSize = 54 + (uint32_t)data.size();
  uint8_t header[54] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,  40,
                        0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0,  1, 0, 32, 0};
  *(uint32_t *)(header + 2) = fileSize;
  *(int32_t *)(header + 18) = width;
  *(int32_t *)(header + 22) = -height; // Top-down
  *(uint32_t *)(header + 34) = (uint32_t)data.size();

  f.write((char *)header, 54);
  f.write((char *)data.data(), data.size());
}

// Scanning Thread - Runs continuously
void ScanningThread() {
  DXGICapture dxgiCapture;
  WindowCapture winCapture;
  TensorRTInference inference;

  bool usingWindowCapture = config.useWindowCapture;
  std::string lastWindowTitle = config.captureWindowTitle;

  std::cout << "[Scanning] Initializing capture at 320x320 (Turbo Mode)"
            << std::endl;
  AddWebLog("[System] Initializing capture at 320x320 (Turbo Mode)");

  auto initCapture = [&]() -> bool {
    // 0: DXGI, 1: BitBlt, 2: Camera
    if (config.captureMethod == 0) {
      if (dxgiCapture.Initialize(config.monitorId, 320, 320)) {
        g_activeCaptureEngine = "DXGI Desktop";
        AddWebLog("[System] DXGI Capture initialized (Monitor " +
                  std::to_string(config.monitorId) + ")");
        return true;
      }
      g_activeCaptureEngine = "DXGI (FAILED)";
      AddWebLog("[Error] DXGI Capture failed. Valorant Fullscreen?");
      return false;
    } else if (config.captureMethod == 1) {
      if (winCapture.Initialize(config.captureWindowTitle, 320, 320)) {
        g_activeCaptureEngine = "BitBlt Window";
        AddWebLog("[System] Window Capture initialized: " +
                  config.captureWindowTitle);
        return true;
      }
      g_activeCaptureEngine = "BitBlt (FAILED)";
      AddWebLog("[Warning] Failed to find window: " +
                config.captureWindowTitle + ". Retrying...");
      return false;
    } else if (config.captureMethod == 2) {
      // CameraCapture is static-based in screen.cpp
      g_activeCaptureEngine = "External/Experimental";
      AddWebLog("[System] External/Experimental Capture active");
      return true;
    }
    return false;
  };

  g_captureActive = initCapture();

  // Load AI model
  static bool modelLoaded = false;
  if (!modelLoaded) {
    inference.SetInputSize(320); // Turbo mode
    if (!inference.LoadModel(config.modelPath)) {
      std::cerr << "[Scanning] Failed to load AI model" << std::endl;
      g_tensorrtLoaded = false;
      return;
    }
    g_tensorrtLoaded = true;
    g_cudaAvailable = true;
    modelLoaded = true;
    AddWebLog("[System] AI Model loaded successfully");
  }

  auto lastFrame = std::chrono::high_resolution_clock::now();
  int frameCount = 0;
  int failCount = 0;

  // Pre-allocate buffers outside loop to eliminate heap churn (lag)
  std::vector<uint8_t> frameData;
  std::vector<Detection> detections;
  int width = 320, height = 320;

  static int lastCaptureMethod = -1;
  while (!unload) { // Check unload flag
    // Check for config changes during runtime
    if (lastCaptureMethod != config.captureMethod ||
        lastWindowTitle != config.captureWindowTitle) {
      std::cout << "[Scanning] Capture settings changed, re-initializing..."
                << std::endl;
      dxgiCapture.Shutdown();
      winCapture.Shutdown();
      lastCaptureMethod = config.captureMethod;
      lastWindowTitle = config.captureWindowTitle;
      g_captureActive = initCapture();
    }

    auto frameStart = std::chrono::high_resolution_clock::now();

    // Capture frame
    bool captured = false;

    if (lastCaptureMethod == 0) {
      captured = dxgiCapture.CaptureFrame(frameData, width, height);
    } else if (lastCaptureMethod == 1) {
      captured = winCapture.CaptureFrame(frameData, width, height);
    } else if (lastCaptureMethod == 2) {
      // Use the global captureFrame function from screen.cpp for camera
      unsigned int *pixels = captureFrame(width, height, 0, 0);
      if (pixels) {
        frameData.resize(width * height * 4);
        memcpy(frameData.data(), pixels, width * height * 4);
        delete[] pixels;
        captured = true;
      }
    }

    if (captured) {
      failCount = 0;
      // Run AI inference
      if (inference.RunInference(frameData, width, height, detections)) {
        {
          std::lock_guard<std::mutex> lock(g_detectionsMutex);
          g_detections = detections;
        }
        g_lastInferenceMs = inference.GetLastInferenceTime();
        {
          std::lock_guard<std::mutex> lock(g_debugFrameMutex);
          g_debugFrame = frameData;
          g_newFrameAvailable = true;
        }

        /* PERIODIC SAVE DEACTIVATED FOR PERFORMANCE
        static auto lastSave = std::chrono::steady_clock::now();
        if (std::chrono::steady_clock::now() - lastSave >
        std::chrono::seconds(3)) { SaveBMP("debug_capture.bmp", frameData,
        width, height, detections); lastSave = std::chrono::steady_clock::now();
        }
        */
      }
    } else {
      failCount++;
      if (failCount > 100) { // If it fails consistently
        g_captureActive = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        g_captureActive = initCapture();
        failCount = 0;
      }
    }

    // FPS and sleep logic...
    frameCount++;
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - lastFrame)
            .count();
    if (elapsed >= 1) {
      g_currentFPS = frameCount;
      frameCount = 0;
      lastFrame = now;
    }

    auto frameEnd = std::chrono::high_resolution_clock::now();
    auto frameDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                             frameEnd - frameStart)
                             .count();

    long long targetMicros = std::max(1, config.detectionIntervalMs) * 1000;
    if (config.detectionIntervalMs <= 1)
      targetMicros = 6944; // ~144 FPS

    long long sleepMicros = targetMicros - frameDuration;
    if (sleepMicros > 0) {
      if (sleepMicros > 1000) {
        std::this_thread::sleep_for(std::chrono::microseconds(sleepMicros));
      } else {
        auto waitStart = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::high_resolution_clock::now() - waitStart)
                   .count() < sleepMicros) {
          std::this_thread::yield();
        }
      }
    }

    // Every ~5 seconds, log a heartbeat to the web UI
    static auto lastHeartbeat = std::chrono::steady_clock::now();
    if (std::chrono::steady_clock::now() - lastHeartbeat >
        std::chrono::seconds(5)) {
      extern void LogToWeb(const std::string &msg);
      LogToWeb("[Scanner] Live - " +
               std::string(config.useWindowCapture ? "Window" : "DXGI") +
               " Mode - " + std::to_string(g_currentFPS.load()) + " FPS");
      lastHeartbeat = std::chrono::steady_clock::now();
    }
  }
}

// Aiming Thread - Only active when aim key is held
void AimingThread() {
  std::cout << "[Aiming] Thread started" << std::endl;

  auto lastFrame = std::chrono::high_resolution_clock::now();
  Detection *lastTarget = nullptr;
  bool hadTarget = false;
  std::vector<Detection> detections;
  bool isShooting = false;
  auto shootStartTime = std::chrono::steady_clock::now();

  while (!unload) { // Check unload flag
    auto frameStart = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(frameStart - lastFrame).count();
    dt = std::max(0.0005f, std::min(dt, 0.05f));

    bool aimKeyHeld =
        (GetAsyncKeyState(config.aimKeyCode) & 0x8000) ||
        (config.aimKeyCode == 164 && (GetAsyncKeyState(18) & 0x8000));

    bool flickKeyHeld = config.flickbotEnabled &&
                        (GetAsyncKeyState(config.flickKeyCode) & 0x8000);
    static int flickbotState = 0; // 0: Idle, 1: Flicking, 2: Fired
    if (!flickKeyHeld)
      flickbotState = 0;

    // Priority: Flickbot takes precedence over normal aimbot if active.
    // If flickbot is in state 2 (Fired), it stops ALL aiming on that key hold.
    if (flickKeyHeld) {
      isAimingActive = (flickbotState < 2);
    } else {
      isAimingActive = aimKeyHeld;
    }

    bool shootingNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (shootingNow && !isShooting)
      shootStartTime = frameStart;
    isShooting = shootingNow;

    if (isAimingActive && gHidLink && gHidLink->IsConnected()) {
      {
        std::lock_guard<std::mutex> lock(g_detectionsMutex);
        detections = g_detections;
      }

      if (!detections.empty()) {
        Detection *bestTarget = nullptr;
        float bestScore = FLT_MAX;

        for (auto &det : detections) {
          float dx = det.x - CENTER_X;
          float dy = det.y - CENTER_Y;
          float dist = std::sqrt(dx * dx + dy * dy);
          if (dist > config.fovRadius)
            continue;

          // Selection Score: Prioritize Heads (Class 1) over Bodies (Class 0)
          float score = dist;
          if (det.classId == 1)
            score -= 2000.0f; // Heads are much more attractive

          // Filtering
          bool flickActive =
              (flickKeyHeld && (flickbotState == 0 || flickbotState == 1 ||
                                flickbotState == 2));
          if (!flickActive) {
            // Normal Aim: Relaxed filtering
            // If AimMode is "Head", we prefer class 1 but accept class 0.
            // If AimMode is "Body", we prefer class 0 but accept class 1 (will
            // target neck/body). So we don't strictly "continue" here unless we
            // want to enforce strictness. For now, let's remove the strict
            // filters to allow fallback.

            // Optional: If you want to STRICTLY ignore heads in Body mode, keep
            // the check. But relying on scores is usually better.
          }

          if (score < bestScore) {
            bestScore = score;
            bestTarget = &det;
          }
        }

        if (flickKeyHeld && flickbotState == 0 && bestTarget) {
          flickbotState = 1;
        }

        if (bestTarget) {
          // ... (Target Speed Estimation & Smoothing logic remains same) ...
          static float lastRawX = 0, lastRawY = 0;
          auto nowTime = std::chrono::steady_clock::now();
          if (bestTarget->x != lastRawX || bestTarget->y != lastRawY) {
            float dt_scan =
                std::chrono::duration<float>(nowTime - g_lastUpdateStartTime)
                    .count();
            if (dt_scan > 0.001f && dt_scan < 0.2f) {
              float dx = bestTarget->x - lastRawX;
              float dy = bestTarget->y - lastRawY;
              float moveDist = std::sqrt(dx * dx + dy * dy);
              float rawVelX = dx / dt_scan;
              float rawVelY = dy / dt_scan;
              g_targetVelX = (g_targetVelX * 0.7f) + (rawVelX * 0.3f);
              g_targetVelY = (g_targetVelY * 0.7f) + (rawVelY * 0.3f);
              static int stationaryFrames = 0;
              if (moveDist < 3.0f)
                stationaryFrames++;
              else
                stationaryFrames = 0;
              if (stationaryFrames > 3) {
                g_targetVelX = 0;
                g_targetVelY = 0;
              }
            }
            lastRawX = bestTarget->x;
            lastRawY = bestTarget->y;
            g_lastUpdateStartTime = nowTime;
          }

          // Phase 35: High-Fidelity Anti-Jitter EMA Filter
          // Replaces conditional "notchy" filter with continuous centering
          // logic.
          static float filterX = bestTarget->x;
          static float filterY = bestTarget->y;

          // Phase 35: Dynamic Smoothing Curve
          // Heavy smoothing when close (anti-shake), raw input when far (fast
          // snap)
          float relX = filterX - CENTER_X;
          float relY = filterY - CENTER_Y;
          float aimDist = std::sqrt(relX * relX + relY * relY);
          float distFactor = std::max(0.0f, std::min(1.0f, aimDist / 20.0f));

          float agilityBias = std::max(0.1f, config.agilityFactor);
          // Curve: 0.1 at 0px -> 0.8 at 20px (scaled by agility)
          float dynamicAlpha = 0.1f + (0.7f * distFactor);
          float alpha_lpf = std::min(0.95f, dynamicAlpha * agilityBias * 0.6f);

          filterX =
              (filterX * (1.0f - alpha_lpf)) + (bestTarget->x * alpha_lpf);
          filterY =
              (filterY * (1.0f - alpha_lpf)) + (bestTarget->y * alpha_lpf);

          g_smoothedTargetX = filterX;
          g_smoothedTargetY = filterY;

          float headX = g_smoothedTargetX - CENTER_X;
          float headY = g_smoothedTargetY - CENTER_Y;
          float targetH = bestTarget->h;

          // Dynamic Head Pivot (Centered in Cyan Circle + 2D UI Adjustment)
          bool useHeadAim =
              (config.aimMode == "Head" || config.aimMode == "Nearest");

          if (bestTarget->classId == 1) {
            // Head (Class 1): Detected Head Bone
            // Always use Head Offsets
            headX = g_smoothedTargetX - (CENTER_X - config.headOffsetX);
            headY = g_smoothedTargetY - (CENTER_Y - config.headOffsetY);
          } else {
            // Body (Class 0): Detected Full Body
            if (useHeadAim) {
              // Aiming for Head using Body Box -> Lift 45% + Head Sliders
              headX = g_smoothedTargetX - (CENTER_X - config.headOffsetX);
              headY -= (targetH * 0.45f - config.headOffsetY);
            } else {
              // Aiming for Body using Body Box -> Lift 10% (Chest) + Body
              // Sliders
              headX = g_smoothedTargetX - (CENTER_X - config.bodyOffsetX);
              // 0.10f lifts slightly to upper chest/neck area
              headY -= (targetH * 0.10f - config.bodyOffsetY);
            }
          }

          int moveX = 0, moveY = 0;
          calculateAimMove(headX, headY, moveX, moveY, dt, isShooting);

          if (isShooting)
            applyRCS(moveX, moveY, dt, true, shootStartTime);

          // Flickbot MB1 Trigger: Dynamic Hitbox Check
          int clickBtn = 0;
          if (flickKeyHeld && flickbotState == 1) {
            float distToHead = std::sqrt(headX * headX + headY * headY);
            // Phase 23: Fire if crosshair is within the adaptive head radius
            // (headHitboxScale)
            float triggerRadius = targetH * config.headHitboxScale;
            if (distToHead < triggerRadius) {
              clickBtn = 1; // MB1 Down (HID protocol handles the released state
                            // internally or needs separate call?)
              // HidLink::WriteCommand sends 'btn' as the button state.
              // We should send button 0 shortly after or rely on the hardware
              // to pulse it. In our HidLink, 'btn' is passed directly.
              flickbotState = 2; // Fired
            }
          }

          if (moveX != 0 || moveY != 0 || clickBtn != 0)
            gHidLink->WriteCommand(moveX, moveY, clickBtn);

          if (clickBtn != 0) {
            // Send release immediately to ensure single click
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            gHidLink->WriteCommand(0, 0, 0);
          }

          hadTarget = true;
        } else if (hadTarget) {
          resetAimingState();
          hadTarget = false;
        }
      } else if (hadTarget) {
        resetAimingState();
        hadTarget = false;
      }
    } else if (hadTarget) {
      resetAimingState();
      hadTarget = false;
    }

    lastFrame = frameStart;
    auto workDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - frameStart)
            .count();
    long long targetMicros = 1000;
    long long sleepMicros = targetMicros - workDuration;
    if (sleepMicros > 2000) {
      std::this_thread::sleep_for(std::chrono::microseconds(sleepMicros));
    } else if (sleepMicros > 0) {
      // Precise busy-wait for sub-millisecond accuracy
      auto waitStart = std::chrono::high_resolution_clock::now();
      while (std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::high_resolution_clock::now() - waitStart)
                 .count() < sleepMicros) {
        std::this_thread::yield();
      }
    }
  }
}

void MonitorThread() {
  while (!unload) {
    g_gpuMemoryPercent = 45.0f;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

int main() {
  try {
    std::cout << "[Debug] Application Start" << std::endl;
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetProcessAffinityMask(GetCurrentProcess(), 0x0F);
    loadConfig();
    gHidLink = new HidLink(config.mouseVID, config.mousePID);

    // Start Application Threads
    std::cout << "[Debug] Starting threads..." << std::endl;

    std::thread scanThread([]() {
      try {
        ScanningThread();
      } catch (const std::system_error &e) {
        std::cerr << "[CRASH] ScanningThread system_error: " << e.what()
                  << " Code: " << e.code() << std::endl;
      } catch (const std::exception &e) {
        std::cerr << "[CRASH] ScanningThread exception: " << e.what()
                  << std::endl;
      } catch (...) {
        std::cerr << "[CRASH] ScanningThread unknown exception" << std::endl;
      }
    });

    std::thread aimThread([]() {
      try {
        AimingThread();
      } catch (const std::system_error &e) {
        std::cerr << "[CRASH] AimingThread system_error: " << e.what()
                  << " Code: " << e.code() << std::endl;
      } catch (const std::exception &e) {
        std::cerr << "[CRASH] AimingThread exception: " << e.what()
                  << std::endl;
      } catch (...) {
        std::cerr << "[CRASH] AimingThread unknown exception" << std::endl;
      }
    });

    std::thread monitorThread([]() {
      try {
        MonitorThread();
      } catch (...) {
      }
    });

    // Run GUI (Main Thread)
    std::cout << "[Debug] Starting GUI..." << std::endl;
    Gui::RenderLoop();
    std::cout << "[Debug] GUI exited." << std::endl;

    // Cleanup
    unload = true;
    if (scanThread.joinable())
      scanThread.join();
    if (aimThread.joinable())
      aimThread.join();
    if (monitorThread.joinable())
      monitorThread.join();

    if (gHidLink) {
      delete gHidLink;
      gHidLink = nullptr;
    }
    return 0;

  } catch (const std::system_error &e) {
    std::cerr << "[CRASH] Main System Error: " << e.what()
              << " Code: " << e.code() << std::endl;
    MessageBoxA(NULL, (std::string("System Error: ") + e.what()).c_str(),
                "Crash", MB_ICONERROR);
    return -1;
  } catch (const std::exception &e) {
    std::cerr << "[CRASH] Main Exception: " << e.what() << std::endl;
    MessageBoxA(NULL, (std::string("Exception: ") + e.what()).c_str(), "Crash",
                MB_ICONERROR);
    return -1;
  } catch (...) {
    std::cerr << "[CRASH] Main Unknown Exception" << std::endl;
    MessageBoxA(NULL, "Unknown Exception", "Crash", MB_ICONERROR);
    return -1;
  }
}
