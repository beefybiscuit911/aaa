#include "hid_link.hpp"
#include <hidclass.h>
#include <hidsdi.h>
#include <iostream>
#include <setupapi.h>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

HidLink::HidLink(int vid, int pid)
    : hDevice(INVALID_HANDLE_VALUE), connected(false) {
  connected = FindHidDevice(vid, pid);
}

HidLink::~HidLink() {
  if (connected && hDevice != INVALID_HANDLE_VALUE) {
    CloseHandle(hDevice);
  }
}

bool HidLink::IsConnected() { return connected; }

std::string HidLink::GetDeviceDetails() {
  if (connected)
    return "RawHID Device (VID:0x1D57 PID:0xFA60)";
  return "Disconnected";
}

bool HidLink::FindHidDevice(int vid, int pid) {
  GUID hidGuid;
  HidD_GetHidGuid(&hidGuid);

  std::cout << "[HID] Searching for VID:0x" << std::hex << vid << " PID:0x"
            << pid << std::dec << "..." << std::endl;

  HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(
      &hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (deviceInfoSet == INVALID_HANDLE_VALUE)
    return false;

  SP_DEVICE_INTERFACE_DATA interfaceData;
  interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid,
                                                i, &interfaceData);
       i++) {
    DWORD detailSize = 0;
    SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &interfaceData, NULL, 0,
                                     &detailSize, NULL);

    std::vector<char> detailBuffer(detailSize);
    PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData =
        (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)detailBuffer.data();
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

    if (SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &interfaceData,
                                         detailData, detailSize, NULL, NULL)) {
      std::string path = detailData->DevicePath;

      HANDLE hTemp = CreateFileA(
          detailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
      if (hTemp != INVALID_HANDLE_VALUE) {
        HIDD_ATTRIBUTES attributes;
        attributes.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(hTemp, &attributes)) {
          if (attributes.VendorID == vid && attributes.ProductID == pid) {
            PHIDP_PREPARSED_DATA preparsedData;
            if (HidD_GetPreparsedData(hTemp, &preparsedData)) {
              HIDP_CAPS caps;
              if (HidP_GetCaps(preparsedData, &caps) == HIDP_STATUS_SUCCESS) {
                // NicoHood RawHID: UsagePage 0xFFC0
                // Standard RawHID: UsagePage 0xFF00
                bool isNicoHood = (caps.UsagePage == 0xFFC0);
                bool isGenericRaw = (caps.UsagePage == 0xFF00);

                // Usually mi_00/mi_01 are standard mouse/keyboard.
                // We STRICTLY want the vendor interface, BUT some NicoHood
                // setups use mi_00 for RawHID.
                bool isStandardMouse =
                    (path.find("mi_00") != std::string::npos &&
                     caps.UsagePage != 0xFFC0);

                if ((isNicoHood || isGenericRaw) && !isStandardMouse) {
                  hDevice = hTemp;
                  devicePath = path;
                  outputReportLength = caps.OutputReportByteLength;
                  m_packetBuffer.assign(outputReportLength, 0);
                  HidD_FreePreparsedData(preparsedData);
                  SetupDiDestroyDeviceInfoList(deviceInfoSet);
                  std::cout
                      << "[HID] SUCCESS: Bound to RawHID interface (PAGE:0x"
                      << std::hex << caps.UsagePage << ")." << std::dec
                      << std::endl;
                  return true;
                }
              }
              HidD_FreePreparsedData(preparsedData);
            }
          }
        }
        CloseHandle(hTemp);
      }
    }
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  return false;
}

void HidLink::WriteCommand(int x, int y, int btn) {
  if (!connected || hDevice == INVALID_HANDLE_VALUE || outputReportLength == 0)
    return;

  // Use pre-allocated packet buffer (Allocation-FREE)
  if (m_packetBuffer.size() != outputReportLength)
    m_packetBuffer.assign(outputReportLength, 0);

  m_packetBuffer[0] = 0x00;                       // Report ID
  m_packetBuffer[1] = 0xA5;                       // Magic
  m_packetBuffer[2] = 0x01;                       // Command (Move/Click)
  m_packetBuffer[3] = (uint8_t)((x >> 8) & 0xFF); // X High
  m_packetBuffer[4] = (uint8_t)(x & 0xFF);        // X Low
  m_packetBuffer[5] = (uint8_t)((y >> 8) & 0xFF); // Y High
  m_packetBuffer[6] = (uint8_t)(y & 0xFF);        // Y Low
  m_packetBuffer[7] = (uint8_t)(btn & 0xFF);      // Buttons
  m_packetBuffer[8] = 0x5A;                       // End

  DWORD bytesWritten = 0;
  if (!WriteFile(hDevice, m_packetBuffer.data(), (DWORD)m_packetBuffer.size(),
                 &bytesWritten, NULL)) {
    static auto lastErrorLog = std::chrono::steady_clock::now();
    if (std::chrono::steady_clock::now() - lastErrorLog >
        std::chrono::seconds(2)) {
      std::cerr << "[HID] WriteFile FAILED! Error: " << GetLastError()
                << " Path: " << devicePath << " (Length: " << outputReportLength
                << ")" << std::endl;
      lastErrorLog = std::chrono::steady_clock::now();
    }
  }
}
