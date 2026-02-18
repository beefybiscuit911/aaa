#include "webserver.hpp"
#include "config.hpp"
#include "globals.hpp"
#include <algorithm>
#include <string>

#undef max
#undef min
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Global stats (will be updated by main threads)
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
extern std::vector<std::string> g_webLogs;
extern std::mutex g_logMutex;

WebServer::WebServer(int port)
    : port(port), running(false), serverSocket(INVALID_SOCKET) {
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "[WebServer] WSAStartup failed" << std::endl;
  }
}

WebServer::~WebServer() {
  Stop();
  WSACleanup();
}

void WebServer::Start() {
  running = true;
  serverThread = std::thread(&WebServer::ServerLoop, this);
}

void WebServer::Stop() {
  running = false;
  if (serverSocket != INVALID_SOCKET) {
    closesocket(serverSocket);
    serverSocket = INVALID_SOCKET;
  }
  if (serverThread.joinable())
    serverThread.join();
}

void WebServer::ServerLoop() {
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(serverSocket, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    std::cerr << "[WebServer] Bind failed with error: " << WSAGetLastError()
              << std::endl;
    closesocket(serverSocket);
    return;
  }
  listen(serverSocket, 5);

  std::cout << "[WebServer] Started on port " << port << std::endl;

  while (running) {
    SOCKET client = accept(serverSocket, nullptr, nullptr);
    if (client != INVALID_SOCKET) {
      // Handle each client in a separate thread to prevent blocking
      std::thread clientThread(&WebServer::HandleClient, this, client);
      clientThread.detach();
    }
  }
}

void WebServer::HandleClient(SOCKET client) {
  std::vector<char> buffer(16384);
  int bytes = recv(client, buffer.data(), 16384, 0);
  if (bytes > 0) {
    std::string request(buffer.data(), bytes);
    std::string method = request.substr(0, request.find(' '));
    std::string path = request.substr(request.find(' ') + 1);
    path = path.substr(0, path.find(' '));

    // Route handling
    if (path == "/" || path == "/index.html") {
      ServeFile(client, "webroot/index.html", "text/html");
    } else if (path == "/style.css") {
      ServeFile(client, "webroot/style.css", "text/css");
    } else if (path == "/app.js") {
      ServeFile(client, "webroot/app.js", "application/javascript");
    } else if (path == "/sw.js") {
      ServeFile(client, "webroot/sw.js", "application/javascript");
    } else if (path == "/manifest.json") {
      ServeFile(client, "webroot/manifest.json", "application/json");
    } else if (path == "/icon-192.png") {
      ServeFile(client, "webroot/icon-192.png", "image/png");
    } else if (path == "/icon-512.png") {
      ServeFile(client, "webroot/icon-512.png", "image/png");
    } else if (path == "/favicon.ico") {
      ServeFile(client, "webroot/favicon.ico", "image/x-icon");
    } else if (path == "/csgo-headshot-logo-png_seeklogo-340259.png") {
      ServeFile(client, "webroot/csgo-headshot-logo-png_seeklogo-340259.png",
                "image/png");
    } else if (path.find("/api/status") != std::string::npos) {
      SendResponse(client, "200 OK", "application/json", GetStatusJSON());
    } else if (path == "/api/config" && method == "GET") {
      SendResponse(client, "200 OK", "application/json", GetConfigJSON());
    } else if (path == "/api/config" && method == "POST") {
      // Extract body from request
      size_t bodyStart = request.find("\r\n\r\n");
      if (bodyStart != std::string::npos) {
        std::string body = request.substr(bodyStart + 4);
        if (UpdateConfigFromJSON(body)) {
          SendResponse(client, "200 OK", "application/json",
                       "{\"success\":true}");
        } else {
          SendResponse(client, "500 Internal Server Error", "application/json",
                       "{\"success\":false, \"error\": \"Failed to parse "
                       "configuration\"}");
        }
      } else {
        SendResponse(client, "400 Bad Request", "text/plain",
                     "Invalid request");
      }
    } else if (path.find("/api/debug/screenshot") == 0) {
      std::string bmp = GetDebugScreenshot();
      if (!bmp.empty()) {
        std::string headers = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: image/bmp\r\n"
                              "Content-Length: " +
                              std::to_string(bmp.size()) +
                              "\r\n"
                              "Cache-Control: no-store, no-cache, "
                              "must-revalidate, proxy-revalidate, max-age=0\r\n"
                              "Pragma: no-cache\r\n"
                              "Expires: 0\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Connection: close\r\n\r\n";

        send(client, headers.c_str(), static_cast<int>(headers.length()), 0);
        send(client, bmp.data(), static_cast<int>(bmp.size()), 0);
      } else {
        SendResponse(client, "503 Service Unavailable", "text/plain",
                     "No frame available");
      }
    } else if (request.find("GET /api/logs") != std::string::npos) {
      SendResponse(client, "200 OK", "application/json", GetLogsJSON());
    } else if (path.find("/assets/") == 0) {
      // Serve files from assets directory for React app
      std::string filename = path.substr(8); // Remove "/assets/"
      std::string filepath = "webroot/assets/" + filename;
      std::string contentType = "application/octet-stream";

      // Determine content type
      if (filename.find(".js") != std::string::npos) {
        contentType = "application/javascript";
      } else if (filename.find(".css") != std::string::npos) {
        contentType = "text/css";
      }

      ServeFile(client, filepath, contentType);
    } else {
      SendResponse(client, "404 Not Found", "text/plain", "Not Found");
    }
  }
  closesocket(client); // Critical: Close socket here since we are in a thread
}

void WebServer::ServeFile(SOCKET client, const std::string &filepath,
                          const std::string &contentType) {
  std::ifstream file(filepath, std::ios::binary);
  if (file.is_open()) {
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    SendResponse(client, "200 OK", contentType, content);
  } else {
    SendResponse(client, "404 Not Found", "text/plain", "File not found");
  }
}

void WebServer::SendResponse(SOCKET client, const std::string &status,
                             const std::string &contentType,
                             const std::string &body) {
  std::string response =
      "HTTP/1.1 " + status + "\r\n" + "Content-Type: " + contentType + "\r\n" +
      "Content-Length: " + std::to_string(body.length()) + "\r\n" +
      "Cache-Control: no-store, no-cache, must-revalidate, proxy-revalidate, "
      "max-age=0\r\n" +
      "Pragma: no-cache\r\n" + "Expires: 0\r\n" +
      "Access-Control-Allow-Origin: *\r\n" + "Connection: close\r\n\r\n" + body;
  send(client, response.c_str(), static_cast<int>(response.length()), 0);
}

std::string WebServer::GetStatusJSON() {
  std::stringstream ss;
  ss << "{";
  ss << "\"fps\": " << g_currentFPS.load() << ",";
  ss << "\"gpu\": " << g_gpuMemoryPercent.load() << ",";
  ss << "\"inference_ms\": " << g_lastInferenceMs.load() << ",";
  ss << "\"status\": {";
  ss << "\"tensor\": " << (g_tensorrtLoaded.load() ? "true" : "false") << ",";
  ss << "\"cuda\": " << (g_cudaAvailable.load() ? "true" : "false") << ",";
  ss << "\"capture\": " << (g_captureActive.load() ? "true" : "false") << ",";
  ss << "\"capture_engine\": \"" << g_activeCaptureEngine << "\"";
  ss << "}";
  ss << "}";
  return ss.str();
}

std::string WebServer::GetConfigJSON() {
  std::stringstream ss;
  ss << "{";
  ss << "\"modelPath\": \"" << config.modelPath << "\",";
  ss << "\"confidenceThreshold\": " << config.confidenceThreshold << ",";
  ss << "\"nmsThreshold\": " << config.nmsThreshold << ",";
  ss << "\"detectionIntervalMs\": " << config.detectionIntervalMs << ",";
  // Aim Settings
  ss << "\"aimKeyCode\": " << config.aimKeyCode << ",";
  ss << "\"aimMode\": \"" << config.aimMode << "\",";
  ss << "\"fovRadius\": " << config.fovRadius << ",";
  ss << "\"aimSmoothing\": " << config.aimSmoothing << ",";
  ss << "\"aimSpeed\": " << config.aimSpeed << ",";

  // Physics / Shared Aiming
  ss << "\"speed\": " << config.speed << ",";
  ss << "\"speedScaling\": " << config.speedScaling << ",";
  ss << "\"trackSpeed\": " << config.trackSpeed << ",";
  ss << "\"friction\": " << config.friction << ",";
  ss << "\"deadzone\": " << config.deadzone << ",";
  ss << "\"maxStep\": " << config.maxStep << ",";
  ss << "\"accelNormal\": " << config.accelNormal << ",";
  ss << "\"accelBrake\": " << config.accelBrake << ",";
  ss << "\"distanceBlendThreshold\": " << config.distanceBlendThreshold << ",";
  ss << "\"snapSpeed\": " << config.snapSpeed << ",";

  // Humanization
  ss << "\"inertiaBias\": " << config.inertiaBias << ",";
  ss << "\"inertiaMin\": " << config.inertiaMin << ",";
  ss << "\"biasInertiaDecay\": " << config.biasInertiaDecay << ",";
  ss << "\"noiseAmplification\": " << config.noiseAmplification << ",";
  ss << "\"noiseFrequency\": " << config.noiseFrequency << ",";
  ss << "\"bezierCurveSeed\": " << config.bezierCurveSeed << ",";
  ss << "\"bezierStrength\": " << config.bezierStrength << ",";
  ss << "\"linearSlope\": " << config.linearSlope << ",";
  ss << "\"agilityFactor\": " << config.agilityFactor << ",";

  // RCS
  ss << "\"rcsEnabled\": " << (config.rcsEnabled ? "1" : "0") << ",";
  ss << "\"rcsMainSpeed\": " << config.rcsMainSpeed << ",";
  ss << "\"rcsStartSpeed\": " << config.rcsStartSpeed << ",";
  ss << "\"rcsProgressSpeed\": " << config.rcsProgressSpeed << ",";
  ss << "\"rcsDelay\": " << config.rcsDelay << ",";
  ss << "\"bodyOffsetX\": " << config.bodyOffsetX << ",";
  ss << "\"bodyOffsetY\": " << config.bodyOffsetY << ",";
  ss << "\"headOffsetX\": " << config.headOffsetX << ",";
  ss << "\"headOffsetY\": " << config.headOffsetY << ",";
  ss << "\"headshotPriority\": " << config.headshotPriority << ",";
  ss << "\"flickbotEnabled\": " << (config.flickbotEnabled ? "1" : "0") << ",";
  ss << "\"flickKeyCode\": " << config.flickKeyCode << ",";
  ss << "\"headHitboxScale\": " << config.headHitboxScale << ",";
  ss << "\"flickClickThreshold\": " << config.flickClickThreshold;
  ss << "}";
  return ss.str();
}

std::string WebServer::GetLogsJSON() {
  std::lock_guard<std::mutex> lock(g_logMutex);
  std::stringstream ss;
  ss << "[";
  for (size_t i = 0; i < g_webLogs.size(); ++i) {
    // Basic JSON escaping for strings
    std::string escaped = g_webLogs[i];
    size_t pos = 0;
    while ((pos = escaped.find("\"", pos)) != std::string::npos) {
      escaped.replace(pos, 1, "\\\"");
      pos += 2;
    }
    ss << "\"" << escaped << "\"";
    if (i < g_webLogs.size() - 1)
      ss << ",";
  }
  ss << "]";
  return ss.str();
}

bool WebServer::UpdateConfigFromJSON(const std::string &json) {
  try {
    // Improved manual parsing
    auto extractString = [](const std::string &json,
                            const std::string &key) -> std::string {
      size_t pos = json.find("\"" + key + "\":");
      if (pos == std::string::npos)
        return "";
      pos = json.find("\"", pos + key.length() + 3);
      if (pos == std::string::npos)
        return "";
      size_t end = json.find("\"", pos + 1);
      if (end == std::string::npos)
        return "";
      return json.substr(pos + 1, end - pos - 1);
    };

    auto extractFloat = [](const std::string &json,
                           const std::string &key) -> float {
      size_t pos = json.find("\"" + key + "\":");
      if (pos == std::string::npos)
        return 0.0f;
      pos += key.length() + 3;
      // Find end of number (comma, closing brace, or whitespace)
      size_t end = json.find_first_of(",} \t\n\r", pos);
      if (end == std::string::npos)
        end = json.length();
      std::string valStr = json.substr(pos, end - pos);
      return std::stof(valStr);
    };

    auto extractInt = [](const std::string &json,
                         const std::string &key) -> int {
      size_t pos = json.find("\"" + key + "\":");
      if (pos == std::string::npos)
        return 0;
      pos += key.length() + 3;
      size_t end = json.find_first_of(",} \t\n\r", pos);
      if (end == std::string::npos)
        end = json.length();
      std::string valStr = json.substr(pos, end - pos);
      return std::stoi(valStr);
    };

    if (json.find("\"modelPath\":") != std::string::npos)
      config.modelPath = extractString(json, "modelPath");
    if (json.find("\"confidenceThreshold\":") != std::string::npos)
      config.confidenceThreshold = extractFloat(json, "confidenceThreshold");
    if (json.find("\"nmsThreshold\":") != std::string::npos)
      config.nmsThreshold = extractFloat(json, "nmsThreshold");
    if (json.find("\"detectionIntervalMs\":") != std::string::npos)
      config.detectionIntervalMs = extractInt(json, "detectionIntervalMs");
    if (json.find("\"aimKeyCode\":") != std::string::npos)
      config.aimKeyCode = extractInt(json, "aimKeyCode");
    if (json.find("\"aimMode\":") != std::string::npos)
      config.aimMode = extractString(json, "aimMode");
    if (json.find("\"fovRadius\":") != std::string::npos)
      config.fovRadius = extractFloat(json, "fovRadius");
    if (json.find("\"aimSmoothing\":") != std::string::npos)
      config.aimSmoothing = extractFloat(json, "aimSmoothing");
    if (json.find("\"aimSpeed\":") != std::string::npos)
      config.aimSpeed = extractFloat(json, "aimSpeed");

    // Physics
    if (json.find("\"speed\":") != std::string::npos)
      config.speed = extractFloat(json, "speed");
    if (json.find("\"speedScaling\":") != std::string::npos)
      config.speedScaling = extractFloat(json, "speedScaling");
    if (json.find("\"trackSpeed\":") != std::string::npos)
      config.trackSpeed = extractFloat(json, "trackSpeed");
    if (json.find("\"friction\":") != std::string::npos)
      config.friction = extractFloat(json, "friction");
    if (json.find("\"deadzone\":") != std::string::npos)
      config.deadzone = extractFloat(json, "deadzone");
    if (json.find("\"maxStep\":") != std::string::npos)
      config.maxStep = extractFloat(json, "maxStep");
    if (json.find("\"accelNormal\":") != std::string::npos)
      config.accelNormal = extractFloat(json, "accelNormal");
    if (json.find("\"accelBrake\":") != std::string::npos)
      config.accelBrake = extractFloat(json, "accelBrake");

    // Humanization
    if (json.find("\"inertiaBias\":") != std::string::npos)
      config.inertiaBias = extractFloat(json, "inertiaBias");
    if (json.find("\"inertiaMin\":") != std::string::npos)
      config.inertiaMin = extractFloat(json, "inertiaMin");
    if (json.find("\"biasInertiaDecay\":") != std::string::npos)
      config.biasInertiaDecay = extractFloat(json, "biasInertiaDecay");
    if (json.find("\"noiseAmplification\":") != std::string::npos)
      config.noiseAmplification = extractFloat(json, "noiseAmplification");
    if (json.find("\"noiseFrequency\":") != std::string::npos)
      config.noiseFrequency = extractFloat(json, "noiseFrequency");
    if (json.find("\"bezierCurveSeed\":") != std::string::npos)
      config.bezierCurveSeed = extractInt(json, "bezierCurveSeed");
    if (json.find("\"bezierStrength\":") != std::string::npos)
      config.bezierStrength = extractFloat(json, "bezierStrength");
    if (json.find("\"linearSlope\":") != std::string::npos)
      config.linearSlope = extractFloat(json, "linearSlope");
    if (json.find("\"agilityFactor\":") != std::string::npos)
      config.agilityFactor = extractFloat(json, "agilityFactor");

    // RCS
    if (json.find("\"rcsEnabled\":") != std::string::npos)
      config.rcsEnabled = extractInt(json, "rcsEnabled") != 0;
    if (json.find("\"rcsMainSpeed\":") != std::string::npos)
      config.rcsMainSpeed = extractFloat(json, "rcsMainSpeed");
    if (json.find("\"rcsStartSpeed\":") != std::string::npos)
      config.rcsStartSpeed = extractFloat(json, "rcsStartSpeed");
    if (json.find("\"rcsProgressSpeed\":") != std::string::npos)
      config.rcsProgressSpeed = extractFloat(json, "rcsProgressSpeed");
    if (json.find("\"rcsDelay\":") != std::string::npos)
      config.rcsDelay = extractInt(json, "rcsDelay");
    if (json.find("\"bodyOffsetX\":") != std::string::npos)
      config.bodyOffsetX = extractFloat(json, "bodyOffsetX");
    if (json.find("\"bodyOffsetY\":") != std::string::npos)
      config.bodyOffsetY = extractFloat(json, "bodyOffsetY");
    if (json.find("\"headOffsetX\":") != std::string::npos)
      config.headOffsetX = extractFloat(json, "headOffsetX");
    if (json.find("\"headOffsetY\":") != std::string::npos)
      config.headOffsetY = extractFloat(json, "headOffsetY");
    if (json.find("\"captureMethod\":") != std::string::npos)
      config.captureMethod = extractInt(json, "captureMethod");
    if (json.find("\"captureWindowTitle\":") != std::string::npos)
      config.captureWindowTitle = extractString(json, "captureWindowTitle");

    // Flickbot
    if (json.find("\"flickbotEnabled\":") != std::string::npos)
      config.flickbotEnabled = extractInt(json, "flickbotEnabled") != 0;
    if (json.find("\"flickKeyCode\":") != std::string::npos)
      config.flickKeyCode = extractInt(json, "flickKeyCode");
    if (json.find("\"headHitboxScale\":") != std::string::npos)
      config.headHitboxScale = extractFloat(json, "headHitboxScale");
    if (json.find("\"flickClickThreshold\":") != std::string::npos)
      config.flickClickThreshold = extractFloat(json, "flickClickThreshold");

    saveConfig();
    std::cout << "[WebServer] Configuration updated successfully" << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[WebServer] Error parsing configuration: " << e.what()
              << std::endl;
    return false;
  }
}

void LogToWeb(const std::string &msg) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  g_webLogs.push_back(msg);
  if (g_webLogs.size() > 100) {
    g_webLogs.erase(g_webLogs.begin());
  }
}

std::string WebServer::GetDebugScreenshot() {
  std::vector<uint8_t> frame;
  std::vector<Detection> detections;
  extern std::mutex g_detectionsMutex;
  extern std::vector<Detection> g_detections;

  {
    std::lock_guard<std::mutex> lock(g_debugFrameMutex);
    if (g_debugFrame.empty())
      return "";
    frame = g_debugFrame;
  }
  {
    std::lock_guard<std::mutex> lock(g_detectionsMutex);
    detections = g_detections;
  }

  int width = 320;
  int height = 320;

  // Auto-detect resolution from frame buffer size
  if (frame.size() == 640 * 640 * 4) {
    width = 640;
    height = 640;
  } else if (frame.size() == 320 * 320 * 4) {
    width = 320;
    height = 320;
  }

  if (frame.size() >= width * height * 4) {
    for (const auto &det : detections) {
      int x1 = static_cast<int>(det.x - det.w / 2.0f);
      int y1 = static_cast<int>(det.y - det.h / 2.0f);
      int x2 = static_cast<int>(det.x + det.w / 2.0f);
      int y2 = static_cast<int>(det.y + det.h / 2.0f);

      x1 = std::max(0, std::min(x1, width - 1));
      y1 = std::max(0, std::min(y1, height - 1));
      x2 = std::max(0, std::min(x2, width - 1));
      y2 = std::max(0, std::min(y2, height - 1));

      // Draw BBox: Head=Green (1), Body=Red (0)
      uint8_t r = (det.classId == 1) ? 0 : 255;
      uint8_t g = (det.classId == 1) ? 255 : 0;
      uint8_t b = 0;

      for (int x = x1; x <= x2; x++) {
        int idx1 = (y1 * width + x) * 4;
        int idx2 = (y2 * width + x) * 4;
        frame[idx1] = b;
        frame[idx1 + 1] = g;
        frame[idx1 + 2] = r;
        frame[idx2] = b;
        frame[idx2 + 1] = g;
        frame[idx2 + 2] = r;
      }
      for (int y = y1; y <= y2; y++) {
        int idx1 = (y * width + x1) * 4;
        int idx2 = (y * width + x2) * 4;
        frame[idx1] = b;
        frame[idx1 + 1] = g;
        frame[idx1 + 2] = r;
        frame[idx2] = b;
        frame[idx2 + 1] = g;
        frame[idx2 + 2] = r;
      }

      // Draw Flickbot Hitbox if enabled
      if (config.flickbotEnabled) {
        float headX = det.x;
        float headY = det.y;
        float targetH = det.h;

        // Head Pivot Logic: 1=Head, 0=Body (Standardized Centering)
        if (det.classId == 1) {
          headX = det.x + config.headOffsetX;
          headY = det.y + config.headOffsetY;
        } else {
          headX = det.x + config.bodyOffsetX;
          headY = det.y - (targetH * 0.45f - config.bodyOffsetY);
        }

        float radius = targetH * config.headHitboxScale;
        for (int y = (int)-radius; y <= (int)radius; y++) {
          for (int x = (int)-radius; x <= (int)radius; x++) {
            float dist = std::sqrt((float)(x * x + y * y));
            if (std::abs(dist - radius) < 1.0f) {
              int px = (int)(headX + x);
              int py = (int)(headY + y);
              if (px >= 0 && px < width && py >= 0 && py < height) {
                int idx = (py * width + px) * 4;
                frame[idx] = 255;     // B
                frame[idx + 1] = 255; // G
                frame[idx + 2] = 0;   // R
              }
            }
          }
        }
      }
    }
  }

  std::vector<uint8_t> bmp;
  uint32_t fileSize = 54 + width * height * 4;
  bmp.resize(fileSize);
  bmp[0] = 'B';
  bmp[1] = 'M';
  *reinterpret_cast<uint32_t *>(&bmp[2]) = fileSize;
  *reinterpret_cast<uint32_t *>(&bmp[10]) = 54;
  *reinterpret_cast<uint32_t *>(&bmp[14]) = 40;
  *reinterpret_cast<int32_t *>(&bmp[18]) = width;
  *reinterpret_cast<int32_t *>(&bmp[22]) = -height;
  *reinterpret_cast<uint16_t *>(&bmp[26]) = 1;
  *reinterpret_cast<uint16_t *>(&bmp[28]) = 32;
  *reinterpret_cast<uint32_t *>(&bmp[34]) = width * height * 4;
  memcpy(&bmp[54], frame.data(), width * height * 4);
  return std::string(bmp.begin(), bmp.end());
}
