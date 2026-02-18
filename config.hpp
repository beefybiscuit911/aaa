#pragma once
#include <string>
#include <vector>

// Color HSV struct (for legacy color detection)
struct ColorHSV {
  int h_min, s_min, v_min;
  int h_max, s_max, v_max;

  ColorHSV()
      : h_min(0), s_min(0), v_min(0), h_max(180), s_max(255), v_max(255) {}
  ColorHSV(int hmn, int smn, int vmn, int hmx, int smx, int vmx)
      : h_min(hmn), s_min(smn), v_min(vmn), h_max(hmx), s_max(smx), v_max(vmx) {
  }
};

// Hybrid config supporting both legacy and AI systems
struct AppConfig {
  // === NEW AI Model Settings ===
  std::string modelPath;
  float confidenceThreshold;
  float nmsThreshold;
  std::vector<std::string> targetClasses;
  int detectionIntervalMs;

  // === NEW AI Aim Settings ===
  int aimKeyCode;
  std::string aimMode;
  float fovRadius;
  float aimSmoothing;
  float aimSpeed;

  // === LEGACY Color Detection ===
  std::string targetColor;
  int tolerance;
  int scanAreaX;
  int scanAreaY;
  bool use_hsv_filtering;
  ColorHSV hsv_purple;
  ColorHSV hsv_red;
  ColorHSV hsv_yellow;

  // === LEGACY Triggerbot ===
  bool triggerEnabled;
  int triggerKeyCode;
  int triggerScanAreaX;
  int triggerScanAreaY;
  int triggerOffset;
  int triggerDelay;
  int shotDelay;
  bool triggerSplitCheck;
  int triggerRequiredPixels;

  // === LEGACY Flickbot ===
  bool flickbotEnabled;
  int flickKeyCode;
  float headHitboxScale;
  float flickClickThreshold;

  // === LEGACY RCS ===
  bool rcsEnabled;
  int rcsDelay;
  float rcsStartSpeed;
  float rcsProgressSpeed;
  float rcsMainSpeed;
  float rcsStrengthX;
  float rcsStrengthY;

  // === Shared Aiming Settings ===
  float speed;
  float speedScaling;
  float trackSpeed;
  float friction;
  float deadzone;
  float maxStep;
  float accelNormal;
  float accelBrake;
  float distanceBlendThreshold;
  float snapSpeed;
  float agilityFactor;

  // === Features ===
  bool enableDualZone;
  float smoothFov;
  float smoothFactor;
  bool enableAntiSmoke;
  bool enableSilentAim;

  // === Humanization (New) ===
  float inertiaBias;
  float inertiaMin;
  float biasInertiaDecay;
  float noiseAmplification;
  float noiseFrequency;
  int bezierCurveSeed;
  float bezierStrength;
  float linearSlope;

  // === Capture ===
  int captureMethod; // 0: DXGI, 1: BitBlt, 2: Camera
  bool useWindowCapture;
  std::string captureWindowTitle;

  // === Validation ===
  int minPixelCount;
  float maxAspectRatio;
  float minDensity;

  // === Headshot / Offsets ===
  float bodyOffsetX;
  float bodyOffsetY;
  float headOffsetX;
  float headOffsetY;
  int headshotPriority;

  // === Hardware ===
  int mouseVID;
  int mousePID;

  // === System ===
  int webPort;
  int monitorId;
  int refreshRate;

  // Constructor with defaults
  AppConfig()
      : // AI Settings
        modelPath("best.engine"), confidenceThreshold(0.5f), nmsThreshold(0.4f),
        targetClasses({"head", "body"}), detectionIntervalMs(1),
        aimKeyCode(0x02), aimMode("Nearest"), fovRadius(150.0f),
        aimSmoothing(5.0f), aimSpeed(50.0f),
        // Legacy Color
        targetColor("PURPLE"), tolerance(20), scanAreaX(320), scanAreaY(320),
        use_hsv_filtering(false),
        // Legacy Trigger
        triggerEnabled(false), triggerKeyCode(0x06), triggerScanAreaX(10),
        triggerScanAreaY(30), triggerOffset(0), triggerDelay(0), shotDelay(150),
        triggerSplitCheck(false), triggerRequiredPixels(2),
        // Legacy Flickbot
        flickbotEnabled(false), flickKeyCode(0x12), headHitboxScale(0.15f),
        flickClickThreshold(0.05f),
        // Legacy RCS
        rcsEnabled(false), rcsDelay(0), rcsStartSpeed(1.0f),
        rcsProgressSpeed(1.0f), rcsMainSpeed(1.0f), rcsStrengthX(1.0f),
        rcsStrengthY(1.0f),
        // Shared Aiming
        speed(0.5f), speedScaling(1.0f), trackSpeed(0.5f), friction(0.2f),
        deadzone(2.0f), maxStep(15.0f), accelNormal(0.1f), accelBrake(0.2f),
        distanceBlendThreshold(100.0f), snapSpeed(10.0f), agilityFactor(1.0f),
        // Features
        enableDualZone(true), smoothFov(60.0f), smoothFactor(0.2f),
        enableAntiSmoke(true), enableSilentAim(false),
        // Humanization (New)
        inertiaBias(0.5f), inertiaMin(0.1f), biasInertiaDecay(0.2f),
        noiseAmplification(1.0f), noiseFrequency(1.0f), bezierCurveSeed(0),
        bezierStrength(1.0f), linearSlope(1.0f),
        // Capture
        captureMethod(0), useWindowCapture(false),
        captureWindowTitle("Valorant"),
        // Validation
        minPixelCount(3), maxAspectRatio(2.5f), minDensity(0.3f),
        // Headshot / Offsets
        bodyOffsetX(0.0f), bodyOffsetY(0.0f), headOffsetX(0.0f),
        headOffsetY(0.0f), headshotPriority(0),
        // Hardware
        mouseVID(0x1D57), mousePID(0xFA60),
        // System
        webPort(8080), monitorId(0), refreshRate(144) {
    // Default HSV ranges
    hsv_purple = ColorHSV(125, 100, 100, 155, 255, 255);
    hsv_red = ColorHSV(0, 100, 100, 10, 255, 255);
    hsv_yellow = ColorHSV(20, 100, 100, 40, 255, 255);
  }
};

extern AppConfig config;

void loadConfig();
void saveConfig();