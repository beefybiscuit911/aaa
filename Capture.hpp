#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class DXGICapture {
public:
  DXGICapture();
  ~DXGICapture();

  bool Initialize(int monitorId, int width, int height);
  bool CaptureFrame(std::vector<uint8_t> &outBuffer, int &outWidth,
                    int &outHeight);
  void Shutdown();

  bool IsInitialized() const { return m_initialized; }

private:
  bool m_initialized;
  int m_width;
  int m_height;
  int m_offsetX;
  int m_offsetY;

  ComPtr<ID3D11Device> m_device;
  ComPtr<ID3D11DeviceContext> m_context;
  ComPtr<IDXGIOutputDuplication> m_duplication;
  ComPtr<ID3D11Texture2D> m_stagingTexture;
};

class WindowCapture {
public:
  WindowCapture();
  ~WindowCapture();

  bool Initialize(const std::string &windowTitle, int width, int height);
  bool CaptureFrame(std::vector<uint8_t> &outBuffer, int &outWidth,
                    int &outHeight);
  void Shutdown();

  bool IsInitialized() const { return m_initialized; }

private:
  bool m_initialized;
  HWND m_targetHwnd;
  int m_width;
  int m_height;

  // GDI-based for now, but optimized with persistent DCs
  HDC m_hdcScreen;
  HDC m_hdcMem;
  HBITMAP m_hBitmap;
  void *m_pBits;
};
