// Copyright 2026 The Reode Authors
//
// Screenshot: press F12 to save the game screen as a PNG in
// Pictures\Geometry Dash (and, optionally, copy it to the clipboard).
//
// The picture is read from the OpenGL back buffer right before
// CCEGLView::swapBuffers presents the finished frame.

#include <windows.h>
#include <gl/GL.h>
#include <shlobj.h>
#include <wincodec.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "mod_util.h"
#include "reode/reode.h"

namespace {

namespace fs = std::filesystem;
using reode::mod_util::CocosExport;
using SwapBuffers = void (*)(void* self);

constexpr char kSwapBuffers[] = "?swapBuffers@CCEGLView@cocos2d@@UEAAXXZ";
constexpr GLenum kGlBgra = 0x80E1;  // GL_BGRA_EXT
constexpr int kBytesPerPixel = 4;

struct Image {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;  // BGRA, top row first.
};

constinit SwapBuffers g_swap_buffers = nullptr;
constinit reode::mod_util::KeyPress* g_key = nullptr;
constinit bool g_copy_to_clipboard = false;

Image ReadBackBuffer() {
  Image image;
  RECT client = {};
  HWND window = WindowFromDC(wglGetCurrentDC());
  if (window == nullptr || !GetClientRect(window, &client)) return image;
  image.width = client.right - client.left;
  image.height = client.bottom - client.top;
  if (image.width <= 0 || image.height <= 0) return image;

  const std::size_t row = static_cast<std::size_t>(image.width) *
                          kBytesPerPixel;
  std::vector<std::uint8_t> bottom_up(row * image.height);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadBuffer(GL_BACK);
  glReadPixels(0, 0, image.width, image.height, kGlBgra, GL_UNSIGNED_BYTE,
               bottom_up.data());
  // OpenGL rows start at the bottom; image files start at the top.
  image.pixels.resize(bottom_up.size());
  for (int y = 0; y < image.height; ++y) {
    std::memcpy(image.pixels.data() + y * row,
                bottom_up.data() + (image.height - 1 - y) * row, row);
  }
  // The back buffer's alpha is not meaningful; make every pixel opaque.
  for (std::size_t i = 3; i < image.pixels.size(); i += kBytesPerPixel) {
    image.pixels[i] = 0xFF;
  }
  return image;
}

// COM for the current thread, released again only if this call started it.
class ComScope {
 public:
  ComScope()
      : started_(SUCCEEDED(
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {}
  ~ComScope() {
    if (started_) CoUninitialize();
  }
  ComScope(const ComScope&) = delete;
  ComScope& operator=(const ComScope&) = delete;

 private:
  bool started_;
};

bool SavePng(const Image& image, const fs::path& file) {
  ComScope com;
  IWICImagingFactory* factory = nullptr;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
    return false;
  }
  IWICStream* stream = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  const UINT stride = static_cast<UINT>(image.width) * kBytesPerPixel;
  bool ok =
      SUCCEEDED(factory->CreateStream(&stream)) &&
      SUCCEEDED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) &&
      SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr,
                                       &encoder)) &&
      SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
      SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
      SUCCEEDED(frame->Initialize(nullptr)) &&
      SUCCEEDED(frame->SetSize(image.width, image.height)) &&
      SUCCEEDED(frame->SetPixelFormat(&format)) &&
      format == GUID_WICPixelFormat32bppBGRA &&
      SUCCEEDED(frame->WritePixels(
          image.height, stride, static_cast<UINT>(image.pixels.size()),
          const_cast<BYTE*>(image.pixels.data()))) &&
      SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
  if (frame != nullptr) frame->Release();
  if (encoder != nullptr) encoder->Release();
  if (stream != nullptr) stream->Release();
  factory->Release();
  return ok;
}

bool CopyToClipboard(const Image& image) {
  const std::size_t size = image.pixels.size();
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + size);
  if (memory == nullptr) return false;
  auto* header = static_cast<BITMAPINFOHEADER*>(GlobalLock(memory));
  *header = {.biSize = sizeof(BITMAPINFOHEADER),
             .biWidth = image.width,
             .biHeight = -image.height,  // Top row first.
             .biPlanes = 1,
             .biBitCount = 32,
             .biCompression = BI_RGB};
  std::memcpy(header + 1, image.pixels.data(), size);
  GlobalUnlock(memory);
  if (!OpenClipboard(nullptr)) {
    GlobalFree(memory);
    return false;
  }
  EmptyClipboard();
  const bool ok = SetClipboardData(CF_DIB, memory) != nullptr;
  CloseClipboard();
  if (!ok) GlobalFree(memory);
  return ok;
}

fs::path ScreenshotDirectory() {
  PWSTR pictures = nullptr;
  fs::path dir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr,
                                     &pictures))) {
    dir = fs::path(pictures) / L"Geometry Dash";
  }
  CoTaskMemFree(pictures);
  return dir;
}

std::string Utf8(const fs::path& path) {
  std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

void TakeScreenshot() {
  const reode::Mod& mod = reode::Mod::Get();
  Image image = ReadBackBuffer();
  if (image.pixels.empty()) {
    mod.Warn("Could not read the game screen");
    return;
  }
  const fs::path dir = ScreenshotDirectory();
  std::error_code error;
  fs::create_directories(dir, error);
  const auto now = std::chrono::zoned_time(
      std::chrono::current_zone(),
      std::chrono::floor<std::chrono::milliseconds>(
          std::chrono::system_clock::now()));
  const fs::path file =
      dir / std::format(L"GD {:%Y-%m-%d %H-%M-%S}.png", now);
  if (!SavePng(image, file)) {
    mod.Warn("Could not save {}", Utf8(file));
    return;
  }
  mod.Info("Screenshot saved: {} ({}x{})", Utf8(file), image.width,
           image.height);
  if (g_copy_to_clipboard && !CopyToClipboard(image)) {
    mod.Warn("Could not copy the screenshot to the clipboard");
  }
}

void SwapBuffersDetour(void* self) {
  if (g_key->Poll()) TakeScreenshot();
  g_swap_buffers(self);
}

}  // namespace

REODE_MOD_MAIN(mod) {
  reode::mod_util::Settings settings(mod.save_dir());
  // Virtual-key code; 123 = F12. See the Windows "Virtual-Key Codes" list.
  static reode::mod_util::KeyPress key(settings.GetInt(L"Key", VK_F12));
  g_key = &key;
  g_copy_to_clipboard = settings.GetBool(L"CopyToClipboard", true);

  auto target = CocosExport<void*>(kSwapBuffers);
  if (target == nullptr) {
    mod.Error("CCEGLView::swapBuffers not found in cocos2d");
    return std::unexpected(reode::Status::kNotFound);
  }
  auto hook = mod.Hook(target, &SwapBuffersDetour, &g_swap_buffers);
  if (!hook) {
    mod.Error("Hook failed: {}", reode::StatusToString(hook.error()));
    return std::unexpected(hook.error());
  }
  mod.Info("Screenshots (F12 unless changed in settings.ini) go to {}",
           Utf8(ScreenshotDirectory()));
  return {};
}
