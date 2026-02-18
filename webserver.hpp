#pragma once
#include "Inference.hpp"
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>


class WebServer {
public:
  WebServer(int port = 8080);
  ~WebServer();

  void Start();
  void Stop();

private:
  int port;
  bool running;
  SOCKET serverSocket;
  std::thread serverThread;

  void ServerLoop();
  void HandleClient(SOCKET client);
  void ServeFile(SOCKET client, const std::string &filepath,
                 const std::string &contentType);
  void SendResponse(SOCKET client, const std::string &status,
                    const std::string &contentType, const std::string &body);

  std::string GetStatusJSON();
  std::string GetConfigJSON();
  std::string GetLogsJSON();
  std::string GetDebugScreenshot();
  bool UpdateConfigFromJSON(const std::string &json);
};
