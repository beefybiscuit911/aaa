#include "gui.hpp"
#include "Config.h"
#include "Menu.h"
#include "MenuBackground.h"
#include "MenuFonts.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include <d3d11.h>
#include <tchar.h>

#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

// Data
static ID3D11Device *g_pd3dDevice = NULL;
static ID3D11DeviceContext *g_pd3dDeviceContext = NULL;
static IDXGISwapChain *g_pSwapChain = NULL;
static ID3D11RenderTargetView *g_mainRenderTargetView = NULL;

// Definitions for externs used by Menu.cpp and modified imgui.cpp
ID3D11ShaderResourceView *menuBg = NULL;
ImFont *controlFont = NULL;
ImFont *boldMenuFont = NULL;
ImFont *menuFont = NULL;
ImFont *tabFont = NULL;
ImFont *tabFont2 = NULL;
ImFont *tabFont3 = NULL;

// Simple WIC texture loader
HRESULT CreateTextureFromMemory(ID3D11Device *device, const unsigned char *data,
                                size_t size,
                                ID3D11ShaderResourceView **out_srv) {
  printf("[GUI] CreateTextureFromMemory: size=%zu\n", size);
  ComPtr<IWICImagingFactory> factory;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    printf("[GUI] CreateTextureFromMemory: CoCreateInstance failed: 0x%08lX\n",
           (unsigned long)hr);
    return hr;
  }

  ComPtr<IWICStream> stream;
  hr = factory->CreateStream(&stream);
  if (FAILED(hr)) {
    printf("[GUI] CreateTextureFromMemory: CreateStream failed: 0x%08lX\n",
           (unsigned long)hr);
    return hr;
  }

  hr = stream->InitializeFromMemory(const_cast<unsigned char *>(data),
                                    static_cast<DWORD>(size));
  if (FAILED(hr)) {
    printf(
        "[GUI] CreateTextureFromMemory: InitializeFromMemory failed: 0x%08lX\n",
        (unsigned long)hr);
    return hr;
  }

  ComPtr<IWICBitmapDecoder> decoder;
  hr = factory->CreateDecoderFromStream(
      stream.Get(), NULL, WICDecodeMetadataCacheOnDemand, &decoder);
  if (FAILED(hr)) {
    printf("[GUI] CreateTextureFromMemory: CreateDecoderFromStream failed: "
           "0x%08lX\n",
           (unsigned long)hr);
    return hr;
  }

  ComPtr<IWICBitmapFrameDecode> frame;
  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr)) {
    printf("[GUI] CreateTextureFromMemory: GetFrame failed: 0x%08lX\n",
           (unsigned long)hr);
    return hr;
  }

  UINT width, height;
  hr = frame->GetSize(&width, &height);
  if (FAILED(hr)) {
    printf("[GUI] CreateTextureFromMemory: GetSize failed: 0x%08lX\n",
           (unsigned long)hr);
    return hr;
  }
  printf("[GUI] CreateTextureFromMemory: Image dimensions: %ux%u\n", width,
         height);

  WICPixelFormatGUID pixelFormat;
  hr = frame->GetPixelFormat(&pixelFormat);
  if (FAILED(hr))
    return hr;

  ComPtr<IWICFormatConverter> converter;
  hr = factory->CreateFormatConverter(&converter);
  if (FAILED(hr))
    return hr;

  hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                             WICBitmapDitherTypeNone, NULL, 0.0f,
                             WICBitmapPaletteTypeMedianCut);
  if (FAILED(hr))
    return hr;

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  std::vector<unsigned char> pixels(width * height * 4);
  hr = converter->CopyPixels(NULL, width * 4, static_cast<UINT>(pixels.size()),
                             pixels.data());
  if (FAILED(hr))
    return hr;

  D3D11_SUBRESOURCE_DATA subres = {};
  subres.pSysMem = pixels.data();
  subres.SysMemPitch = width * 4;

  ComPtr<ID3D11Texture2D> texture;
  hr = device->CreateTexture2D(&desc, &subres, &texture);
  if (FAILED(hr))
    return hr;

  hr = device->CreateShaderResourceView(texture.Get(), NULL, out_srv);
  return hr;
}
Config g_Config;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

namespace Gui {

void RenderLoop() {
  printf("[GUI] Starting RenderLoop...\n");
  // Initialize COM for WIC
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    printf("[GUI] CoInitializeEx failed: 0x%08lX\n", (unsigned long)hr);
  } else {
    printf("[GUI] COM initialized.\n");
  }
  bool com_initialized = SUCCEEDED(hr);

  // Create application window
  WNDCLASSEX wc = {sizeof(WNDCLASSEX),    CS_CLASSDC, WndProc, 0L,   0L,
                   GetModuleHandle(NULL), NULL,       NULL,    NULL, NULL,
                   _T("ImGui Example"),   NULL};
  RegisterClassEx(&wc);
  HWND hwnd =
      CreateWindow(wc.lpszClassName, _T("ConsoleApplication1 Menu"), WS_POPUP,
                   100, 100, 660, 560, NULL, NULL, wc.hInstance, NULL);

  // Initialize Direct3D
  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return;
  }

  // Show the window
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
  // Keyboard Controls

  // Load Fonts from embedded data
  io.Fonts->AddFontDefault();

  // Load custom fonts from MenuFonts.h
  ImFontConfig font_cfg;
  font_cfg.FontDataOwnedByAtlas = false;

  // Load verdana font (main text)
  menuFont = io.Fonts->AddFontFromMemoryCompressedTTF(
      verdana_compressed_data, verdana_compressed_size, 14.0f, &font_cfg);
  boldMenuFont = io.Fonts->AddFontFromMemoryCompressedTTF(
      verdana_compressed_data, verdana_compressed_size, 14.0f,
      &font_cfg); // Ideally have a bold TTF but we'll use regular for now

  // Load icon font (tabs)
  tabFont = io.Fonts->AddFontFromMemoryCompressedTTF(
      icon_font_compressed_data, icon_font_compressed_size, 34.0f, &font_cfg);
  tabFont2 = io.Fonts->AddFontFromMemoryCompressedTTF(
      icon_font_compressed_data, icon_font_compressed_size, 24.0f, &font_cfg);
  tabFont3 = io.Fonts->AddFontFromMemoryCompressedTTF(
      icon_font_compressed_data, icon_font_compressed_size, 18.0f, &font_cfg);

  // Load combo arrow font (symbols)
  controlFont = io.Fonts->AddFontFromMemoryCompressedTTF(
      comboarrow_compressed_data, comboarrow_compressed_size, 14.0f, &font_cfg);

  io.Fonts->Build(); // Explicitly build the atlas

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

  // Load background texture
  printf("[GUI] Loading background texture...\n");
  hr = CreateTextureFromMemory(g_pd3dDevice, menuBackground,
                               sizeof(menuBackground), &menuBg);
  if (FAILED(hr)) {
    printf("[GUI] CreateTextureFromMemory failed: 0x%08lX\n",
           (unsigned long)hr);
    // Continue even if background fails? No, let's see why it fails.
  } else {
    printf("[GUI] Background texture loaded.\n");
  }

  // Main loop
  printf("[GUI] Entering main loop...\n");
  bool done = false;
  while (!done) {
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        done = true;
    }
    if (done)
      break;

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Render our menu
    Menu::Get().Render();

    // Rendering
    ImGui::Render();
    const float clear_color_with_alpha[4] = {0.45f, 0.55f, 0.60f, 1.00f};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView,
                                               clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pSwapChain->Present(1, 0); // Present with vsync
  }

  // Cleanup
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  CleanupDeviceD3D();
  DestroyWindow(hwnd);
  UnregisterClass(wc.lpszClassName, wc.hInstance);
}

} // namespace Gui

// Helper functions

bool CreateDeviceD3D(HWND hWnd) {
  // Setup swap chain
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };
  if (D3D11CreateDeviceAndSwapChain(
          NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
          featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
          &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
    return false;

  CreateRenderTarget();
  return true;
}

void CleanupDeviceD3D() {
  CleanupRenderTarget();
  if (g_pSwapChain) {
    g_pSwapChain->Release();
    g_pSwapChain = NULL;
  }
  if (g_pd3dDeviceContext) {
    g_pd3dDeviceContext->Release();
    g_pd3dDeviceContext = NULL;
  }
  if (g_pd3dDevice) {
    g_pd3dDevice->Release();
    g_pd3dDevice = NULL;
  }
}

void CreateRenderTarget() {
  ID3D11Texture2D *pBackBuffer;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL,
                                       &g_mainRenderTargetView);
  pBackBuffer->Release();
}

void CleanupRenderTarget() {
  if (g_mainRenderTargetView) {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = NULL;
  }
}

// Forward declare message handler from imgui_impl_win32.cpp
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SIZE:
    if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
      CleanupRenderTarget();
      g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                                  DXGI_FORMAT_UNKNOWN, 0);
      CreateRenderTarget();
    }
    return 0;
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_NCHITTEST:
    // Allow dragging the window by clicking anywhere
    {
      LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
      if (hit == HTCLIENT)
        hit = HTCAPTION;
      return hit;
    }
  case WM_DESTROY:
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
