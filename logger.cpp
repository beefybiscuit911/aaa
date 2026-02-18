#include "logger.hpp"
#include "globals.hpp"
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

std::ofstream SilentLogger::logFile;
std::mutex SilentLogger::logMutex;

void SilentLogger::Init() { logFile.open("colorbot.log", std::ios::app); }

void SilentLogger::Log(const std::string &message) {
  std::lock_guard<std::mutex> lock(logMutex);
  if (logFile.is_open()) {
    logFile << message << std::endl;
  }
}

void SilentLogger::Shutdown() {
  if (logFile.is_open()) {
    logFile.close();
  }
}