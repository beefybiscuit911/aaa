#pragma once
#include <fstream>
#include <mutex>
#include <string>


class SilentLogger {
  static std::ofstream logFile;
  static std::mutex logMutex;

public:
  static void Init();
  static void Log(const std::string &message);
  static void Shutdown();
};