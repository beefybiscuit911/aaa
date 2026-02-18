#include "udp_com.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include <iostream>


#pragma comment(lib, "ws2_32.lib")

UDPClient::UDPClient() : socket_(INVALID_SOCKET), initialized_(false) {}

UDPClient::~UDPClient() { shutdown(); }

bool UDPClient::initialize(const std::string &ip, unsigned short port) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_)
    return true;

  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    return false;
  }

  socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == INVALID_SOCKET) {
    return false;
  }

  // Configure target address
  targetAddr_.sin_family = AF_INET;
  targetAddr_.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &targetAddr_.sin_addr);

  // Set socket to non-blocking
  u_long mode = 1;
  ioctlsocket(socket_, FIONBIO, &mode);

  initialized_ = true;
  return true;
}

void UDPClient::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (socket_ != INVALID_SOCKET) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }
  initialized_ = false;
}

bool UDPClient::sendData(const std::string &data) {
  return sendData(data.c_str(), data.size());
}

bool UDPClient::sendData(const char *data, size_t length) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_ || length == 0)
    return false;

  int result =
      sendto(socket_, data, static_cast<int>(length), 0,
             reinterpret_cast<sockaddr *>(&targetAddr_), sizeof(targetAddr_));

  return result != SOCKET_ERROR;
}