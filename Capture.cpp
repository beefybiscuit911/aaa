#include "Capture.hpp"
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

DXGICapture::DXGICapture()
    : m_initialized(false), m_width(0), m_height(0), m_offsetX(0),
      m_offsetY(0) {}

DXGICapture::~DXGICapture() { Shutdown(); }

bool DXGICapture::Initialize(int monitorId, int width, int height) {
  m_width = width;
  m_height = height;

  // Create D3D11 Device
  D3D_FEATURE_LEVEL featureLevel;
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                 nullptr, 0, D3D11_SDK_VERSION, &m_device,
                                 &featureLevel, &m_context);

  if (FAILED(hr)) {
    std::cerr << "[DXGI] Failed to create D3D11 device" << std::endl;
    return false;
  }

  // Get DXGI Device
  ComPtr<IDXGIDevice> dxgiDevice;
  hr = m_device.As(&dxgiDevice);
  if (FAILED(hr))
    return false;

  // Get DXGI Adapter
  ComPtr<IDXGIAdapter> dxgiAdapter;
  hr = dxgiDevice->GetAdapter(&dxgiAdapter);
  if (FAILED(hr))
    return false;

  // Get Output (Monitor)
  ComPtr<IDXGIOutput> dxgiOutput;
  hr = dxgiAdapter->EnumOutputs(monitorId, &dxgiOutput);
  if (FAILED(hr)) {
    std::cerr << "[DXGI] Failed to get monitor " << monitorId << std::endl;
    return false;
  }

  // Get Output1 for duplication
  ComPtr<IDXGIOutput1> dxgiOutput1;
  hr = dxgiOutput.As(&dxgiOutput1);
  if (FAILED(hr))
    return false;

  // Create Desktop Duplication
  hr = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_duplication);
  if (FAILED(hr)) {
    std::cerr << "[DXGI] Failed to create desktop duplication" << std::endl;
    return false;
  }

  // Get monitor resolution to center the capture area
  DXGI_OUTPUT_DESC outputDesc;
  dxgiOutput->GetDesc(&outputDesc);
  int screenWidth =
      outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
  int screenHeight =
      outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;

  m_offsetX = (screenWidth - m_width) / 2;
  m_offsetY = (screenHeight - m_height) / 2;

  std::cout << "[DXGI] Screen: " << screenWidth << "x" << screenHeight
            << ", Offset: " << m_offsetX << "," << m_offsetY << std::endl;

  // Create staging texture for CPU readback
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = m_width;
  desc.Height = m_height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

  hr = m_device->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
  if (FAILED(hr)) {
    std::cerr << "[DXGI] Failed to create staging texture" << std::endl;
    return false;
  }

  m_initialized = true;
  std::cout << "[DXGI] Initialized successfully" << std::endl;
  return true;
}

bool DXGICapture::CaptureFrame(std::vector<uint8_t> &outBuffer, int &outWidth,
                               int &outHeight) {
  if (!m_initialized)
    return false;

  DXGI_OUTDUPL_FRAME_INFO frameInfo;
  ComPtr<IDXGIResource> desktopResource;

  HRESULT hr = m_duplication->AcquireNextFrame(5, &frameInfo, &desktopResource);
  if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
    return false; // No new frame
  }
  if (FAILED(hr)) {
    std::cerr << "[DXGI] AcquireNextFrame failed" << std::endl;
    return false;
  }

  // Get texture from resource
  ComPtr<ID3D11Texture2D> desktopTexture;
  hr = desktopResource.As(&desktopTexture);
  if (FAILED(hr)) {
    m_duplication->ReleaseFrame();
    return false;
  }

  // Copy to staging texture
  D3D11_BOX sourceRegion;
  sourceRegion.left = m_offsetX;
  sourceRegion.right = m_offsetX + m_width;
  sourceRegion.top = m_offsetY;
  sourceRegion.bottom = m_offsetY + m_height;
  sourceRegion.front = 0;
  sourceRegion.back = 1;

  m_context->CopySubresourceRegion(m_stagingTexture.Get(), 0, 0, 0, 0,
                                   desktopTexture.Get(), 0, &sourceRegion);

  // Map staging texture to CPU
  D3D11_MAPPED_SUBRESOURCE mapped;
  hr = m_context->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr)) {
    m_duplication->ReleaseFrame();
    return false;
  }

  // Copy to output buffer
  outWidth = m_width;
  outHeight = m_height;
  outBuffer.resize(m_width * m_height * 4); // BGRA

  uint8_t *src = static_cast<uint8_t *>(mapped.pData);
  for (int y = 0; y < m_height; ++y) {
    memcpy(&outBuffer[y * m_width * 4], &src[y * mapped.RowPitch], m_width * 4);
  }

  m_context->Unmap(m_stagingTexture.Get(), 0);
  m_duplication->ReleaseFrame();

  return true;
}

void DXGICapture::Shutdown() {
  m_duplication.Reset();
  m_stagingTexture.Reset();
  m_context.Reset();
  m_device.Reset();
  m_initialized = false;
}

// WindowCapture Implementation
WindowCapture::WindowCapture()
    : m_initialized(false), m_targetHwnd(nullptr), m_width(0), m_height(0),
      m_hdcScreen(nullptr), m_hdcMem(nullptr), m_hBitmap(nullptr),
      m_pBits(nullptr) {}

WindowCapture::~WindowCapture() { Shutdown(); }

bool WindowCapture::Initialize(const std::string &windowTitle, int width,
                               int height) {
  m_width = width;
  m_height = height;

  m_targetHwnd = FindWindowA(NULL, windowTitle.c_str());
  if (!m_targetHwnd) {
    // Try partial match if exact fails
    struct EnumData {
      std::string target;
      HWND foundHwnd;
    } data = {windowTitle, NULL};

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
          EnumData *d = (EnumData *)lParam;
          char title[256];
          GetWindowTextA(hwnd, title, sizeof(title));
          std::string strTitle(title);
          if (strTitle.find(d->target) != std::string::npos ||
              strTitle.find("Moonlight") != std::string::npos ||
              strTitle.find("Sunshine") != std::string::npos) {
            d->foundHwnd = hwnd;
            return FALSE; // Stop enumeration
          }
          return TRUE;
        },
        (LPARAM)&data);

    m_targetHwnd = data.foundHwnd;

    if (!m_targetHwnd) {
      std::string msg =
          "[WindowCapt] Could not find window containing: " + windowTitle +
          " or 'Moonlight'";
      std::cerr << msg << std::endl;
      // Also log to web UI
      extern void AddWebLog(const std::string &msg);
      AddWebLog(msg);
      return false;
    }
  }

  m_hdcScreen = GetDC(NULL);
  m_hdcMem = CreateCompatibleDC(m_hdcScreen);

  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = m_width;
  bmi.bmiHeader.biHeight = -m_height; // Top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  m_hBitmap =
      CreateDIBSection(m_hdcMem, &bmi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
  if (!m_hBitmap || !m_pBits) {
    std::cerr << "[WindowCapt] Failed to create DIB section" << std::endl;
    Shutdown();
    return false;
  }

  SelectObject(m_hdcMem, m_hBitmap);

  m_initialized = true;
  std::cout << "[WindowCapt] Initialized for window: " << windowTitle
            << std::endl;
  return true;
}

bool WindowCapture::CaptureFrame(std::vector<uint8_t> &outBuffer, int &outWidth,
                                 int &outHeight) {
  if (!m_initialized || !m_targetHwnd)
    return false;

  if (!IsWindow(m_targetHwnd)) {
    m_initialized = false;
    return false;
  }

  // Get window rect to capture the client area center
  RECT rect;
  GetClientRect(m_targetHwnd, &rect);
  int winWidth = rect.right - rect.left;
  int winHeight = rect.bottom - rect.top;

  POINT pt = {0, 0};
  ClientToScreen(m_targetHwnd, &pt);

  // Center the capture area within the window
  int screenX = pt.x + (winWidth - m_width) / 2;
  int screenY = pt.y + (winHeight - m_height) / 2;

  // Use BitBlt to capture from the screen at the window's location
  // This is faster than PrintWindow and bypasses most anti-captures that target
  // specific windows
  if (!BitBlt(m_hdcMem, 0, 0, m_width, m_height, m_hdcScreen, screenX, screenY,
              SRCCOPY)) {
    return false;
  }

  outWidth = m_width;
  outHeight = m_height;
  outBuffer.resize(m_width * m_height * 4);
  memcpy(outBuffer.data(), m_pBits, m_width * m_height * 4);

  return true;
}

void WindowCapture::Shutdown() {
  if (m_hBitmap)
    DeleteObject(m_hBitmap);
  if (m_hdcMem)
    DeleteDC(m_hdcMem);
  if (m_hdcScreen)
    ReleaseDC(NULL, m_hdcScreen);

  m_targetHwnd = nullptr;
  m_hBitmap = nullptr;
  m_hdcMem = nullptr;
  m_hdcScreen = nullptr;
  m_pBits = nullptr;
  m_initialized = false;
}
