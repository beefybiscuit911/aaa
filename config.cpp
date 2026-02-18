#include "config.hpp"
#include <fstream>
#include <iostream>
#include <mutex>
#include <windows.h>

AppConfig config;
std::recursive_mutex g_configMutex;

// Simple key=value parser (not JSON, easier to implement without dependencies)
void loadConfig() {
  std::lock_guard<std::recursive_mutex> lock(g_configMutex);
  std::ifstream file("config.txt");
  if (!file.is_open()) {
    std::cout << "[Config] config.txt not found, creating with defaults..."
              << std::endl;
    saveConfig();
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Skip comments and empty lines
    if (line.empty() || line[0] == '#')
      continue;

    size_t pos = line.find('=');
    if (pos == std::string::npos)
      continue;

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);

    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    // Parse values
    if (key == "modelPath" && !value.empty())
      config.modelPath = value;
    else if (key == "confidenceThreshold")
      config.confidenceThreshold = std::stof(value);
    else if (key == "nmsThreshold")
      config.nmsThreshold = std::stof(value);
    else if (key == "detectionIntervalMs")
      config.detectionIntervalMs = std::stoi(value);
    else if (key == "aimKeyCode")
      config.aimKeyCode = std::stoi(value);
    else if (key == "aimMode")
      config.aimMode = value;
    else if (key == "fovRadius")
      config.fovRadius = std::stof(value);
    else if (key == "aimSmoothing")
      config.aimSmoothing = std::stof(value);
    else if (key == "aimSpeed")
      config.aimSpeed = std::stof(value);
    else if (key == "deadzone")
      config.deadzone = std::stof(value);
    else if (key == "maxStep")
      config.maxStep = std::stof(value);
    else if (key == "enableSilentAim")
      config.enableSilentAim = (value == "true" || value == "1");
    else if (key == "mouseVID")
      config.mouseVID = std::stoi(value);
    else if (key == "mousePID")
      config.mousePID = std::stoi(value);
    else if (key == "webPort")
      config.webPort = std::stoi(value);
    else if (key == "monitorId")
      config.monitorId = std::stoi(value);
    else if (key == "refreshRate")
      config.refreshRate = std::stoi(value);
    // Aiming parameters
    else if (key == "speed")
      config.speed = std::stof(value);
    else if (key == "speedScaling")
      config.speedScaling = std::stof(value);
    else if (key == "trackSpeed")
      config.trackSpeed = std::stof(value);
    else if (key == "snapSpeed")
      config.snapSpeed = std::stof(value);
    else if (key == "accelNormal")
      config.accelNormal = std::stof(value);
    else if (key == "accelBrake")
      config.accelBrake = std::stof(value);
    else if (key == "distanceBlendThreshold")
      config.distanceBlendThreshold = std::stof(value);
    else if (key == "enableDualZone")
      config.enableDualZone = (value == "true" || value == "1");
    else if (key == "smoothFov")
      config.smoothFov = std::stof(value);
    else if (key == "smoothFactor")
      config.smoothFactor = std::stof(value);
    else if (key == "bezierCurve" || key == "bezierStrength")
      config.bezierStrength = std::stof(value);
    // RCS
    else if (key == "rcsEnabled")
      config.rcsEnabled = (value == "true" || value == "1");
    else if (key == "rcsDelay")
      config.rcsDelay = std::stoi(value);
    else if (key == "rcsStartSpeed")
      config.rcsStartSpeed = std::stof(value);
    else if (key == "rcsProgressSpeed")
      config.rcsProgressSpeed = std::stof(value);
    else if (key == "rcsMainSpeed")
      config.rcsMainSpeed = std::stof(value);
    else if (key == "rcsStrengthX")
      config.rcsStrengthX = std::stof(value);
    else if (key == "friction")
      config.friction = std::stof(value);
    else if (key == "inertiaBias")
      config.inertiaBias = std::stof(value);
    else if (key == "inertiaMin")
      config.inertiaMin = std::stof(value);
    else if (key == "biasInertiaDecay")
      config.biasInertiaDecay = std::stof(value);
    else if (key == "noiseAmplification")
      config.noiseAmplification = std::stof(value);
    else if (key == "noiseFrequency")
      config.noiseFrequency = std::stof(value);
    else if (key == "bezierCurveSeed")
      config.bezierCurveSeed = std::stoi(value);
    else if (key == "bezierStrength")
      config.bezierStrength = std::stof(value);
    else if (key == "linearSlope")
      config.linearSlope = std::stof(value);
    else if (key == "agilityFactor")
      config.agilityFactor = std::stof(value);
    else if (key == "headshotPriority")
      config.headshotPriority = std::stoi(value);
    else if (key == "bodyOffsetX")
      config.bodyOffsetX = std::stof(value);
    else if (key == "bodyOffsetY")
      config.bodyOffsetY = std::stof(value);
    else if (key == "headOffsetX")
      config.headOffsetX = std::stof(value);
    else if (key == "headOffsetY")
      config.headOffsetY = std::stof(value);
    else if (key == "captureMethod")
      config.captureMethod = std::stoi(value);
    else if (key == "useWindowCapture")
      config.useWindowCapture = (value == "true" || value == "1");
    else if (key == "captureWindowTitle")
      config.captureWindowTitle = value;
    else if (key == "flickbotEnabled")
      config.flickbotEnabled = (value == "true" || value == "1");
    else if (key == "flickKeyCode")
      config.flickKeyCode = std::stoi(value);
    else if (key == "headHitboxScale")
      config.headHitboxScale = std::stof(value);
    else if (key == "flickClickThreshold")
      config.flickClickThreshold = std::stof(value);
  }

  file.close();
  std::cout << "[Config] Loaded from config.txt" << std::endl;
}

void saveConfig() {
  std::lock_guard<std::recursive_mutex> lock(g_configMutex);
  std::ofstream file("config.txt");
  if (!file.is_open()) {
    std::cerr << "[Config] Failed to create config.txt" << std::endl;
    return;
  }

  file << "# AI Aimbot Configuration\n\n";

  file << "# AI Model Settings\n";
  file << "modelPath=" << config.modelPath << "\n";
  file << "confidenceThreshold=" << config.confidenceThreshold << "\n";
  file << "nmsThreshold=" << config.nmsThreshold << "\n";
  file << "detectionIntervalMs=" << config.detectionIntervalMs << "\n\n";

  file << "# Aim Settings\n";
  file << "aimKeyCode=" << config.aimKeyCode
       << " # 0x02 = Right Mouse Button\n";
  file << "aimMode=" << config.aimMode << " # Head, Body, or Nearest\n";
  file << "fovRadius=" << config.fovRadius << "\n";
  file << "aimSmoothing=" << config.aimSmoothing << "\n";
  file << "aimSpeed=" << config.aimSpeed << "\n\n";

  file << "# Advanced Aiming\n";
  file << "speed=" << config.speed << "\n";
  file << "speedScaling=" << config.speedScaling << "\n";
  file << "trackSpeed=" << config.trackSpeed << "\n";
  file << "snapSpeed=" << config.snapSpeed << "\n";
  file << "deadzone=" << config.deadzone << "\n";
  file << "maxStep=" << config.maxStep << "\n";
  file << "accelNormal=" << config.accelNormal << "\n";
  file << "accelBrake=" << config.accelBrake << "\n";
  file << "friction=" << config.friction << "\n";
  file << "distanceBlendThreshold=" << config.distanceBlendThreshold << "\n";
  file << "enableDualZone=" << (config.enableDualZone ? "true" : "false")
       << "\n";
  file << "smoothFov=" << config.smoothFov << "\n";
  file << "smoothFactor=" << config.smoothFactor << "\n";
  file << "enableSilentAim=" << (config.enableSilentAim ? "true" : "false")
       << "\n\n";

  file << "# Humanization\n";
  file << "inertiaBias=" << config.inertiaBias << "\n";
  file << "inertiaMin=" << config.inertiaMin << "\n";
  file << "biasInertiaDecay=" << config.biasInertiaDecay << "\n";
  file << "noiseAmplification=" << config.noiseAmplification << "\n";
  file << "noiseFrequency=" << config.noiseFrequency << "\n";
  file << "bezierCurveSeed=" << config.bezierCurveSeed << "\n";
  file << "bezierStrength=" << config.bezierStrength << "\n";
  file << "linearSlope=" << config.linearSlope << "\n";
  file << "agilityFactor=" << config.agilityFactor << "\n\n";

  file << "# Anatomy\n";
  file << "bodyOffsetX=" << config.bodyOffsetX << "\n";
  file << "bodyOffsetY=" << config.bodyOffsetY << "\n";
  file << "headOffsetX=" << config.headOffsetX << "\n";
  file << "headOffsetY=" << config.headOffsetY << "\n";
  file << "headshotPriority=" << config.headshotPriority << "\n\n";

  file << "# Capture Settings\n";
  file << "captureMethod=" << config.captureMethod << "\n";
  file << "useWindowCapture=" << (config.useWindowCapture ? "true" : "false")
       << "\n";
  file << "captureWindowTitle=" << config.captureWindowTitle << "\n\n";
  file << "# Flickbot Settings\n";
  file << "flickbotEnabled=" << (config.flickbotEnabled ? "true" : "false")
       << "\n";
  file << "flickKeyCode=" << config.flickKeyCode << "\n";
  file << "headHitboxScale=" << config.headHitboxScale << "\n";
  file << "flickClickThreshold=" << config.flickClickThreshold << "\n\n";

  file << "# RCS (Recoil Control)\n";
  file << "rcsEnabled=" << (config.rcsEnabled ? "true" : "false") << "\n";
  file << "rcsDelay=" << config.rcsDelay << "\n";
  file << "rcsStartSpeed=" << config.rcsStartSpeed << "\n";
  file << "rcsProgressSpeed=" << config.rcsProgressSpeed << "\n";
  file << "rcsMainSpeed=" << config.rcsMainSpeed << "\n";
  file << "rcsStrengthX=" << config.rcsStrengthX << "\n";
  file << "rcsStrengthY=" << config.rcsStrengthY << "\n\n";

  file << "# Hardware\n";
  file << "mouseVID=" << config.mouseVID << "\n";
  file << "mousePID=" << config.mousePID << "\n\n";

  file << "# System\n";
  file << "webPort=" << config.webPort << "\n";
  file << "monitorId=" << config.monitorId << "\n";
  file << "refreshRate=" << config.refreshRate << "\n";

  file.close();
  std::cout << "[Config] Saved to config.txt" << std::endl;
}
