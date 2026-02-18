// includes.h
#pragma once

// Critical defines to prevent conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent winsock.h from being included
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Include winsock2.h BEFORE windows.h to prevent winsock.h conflicts
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Windows headers - must come after winsock2.h
#include <windows.h>

// Standard includes
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

// Only declarations
extern std::atomic<bool> debugModeActive;
extern std::atomic<bool> captureActive;
