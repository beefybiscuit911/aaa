#pragma once
#include "includes.h"
#include <string>
#include <mutex>
#include <atomic>

class UDPClient {
    SOCKET socket_;
    sockaddr_in targetAddr_;
    std::mutex mutex_;
    std::atomic<bool> initialized_;

public:
    UDPClient();
    ~UDPClient();

    bool initialize(const std::string& ip, unsigned short port);
    void shutdown();
    bool sendData(const std::string& data);
    bool sendData(const char* data, size_t length);
};