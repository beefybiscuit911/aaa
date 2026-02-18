#pragma once
#include "includes.h"
#include <string>
#include <vector>

class HidLink {
public:
  HidLink(int vid, int pid);
  ~HidLink();

  bool IsConnected();
  std::string GetDeviceDetails();
  void WriteCommand(int x, int y, int btn = 0);

private:
  HANDLE hDevice;
  bool connected;
  std::string devicePath;
  size_t outputReportLength;
  std::vector<uint8_t> m_packetBuffer;

  bool FindHidDevice(int vid, int pid);
};
