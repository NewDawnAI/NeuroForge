#include "sandbox/WebSandbox.h"
#include <cstdio> // debug logging for sandbox events
#include <cstdlib>

#ifdef _WIN32
// We need full Windows headers (not lean) for GUID definitions needed by
// knownfolders.h
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Include Windows headers in proper order
#include <guiddef.h> // For GUID type definition
#include <knownfolders.h>
#include <objbase.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <windows.h>
#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "Shell32.lib")
#if defined(NF_HAVE_WEBVIEW2)
#include "sandbox/ComHelpers.h"
#include <WebView2.h>
// No WRL namespace usage
#endif
#endif

namespace NeuroForge {
namespace Sandbox {

#ifdef _WIN32
static constexpr UINT WM_NF_SANDBOX_FLUSH_NAV = WM_APP + 0x4A1;
static constexpr UINT_PTR NF_SANDBOX_NAV_TIMER_ID = 1;
static constexpr DWORD NF_SANDBOX_NAV_RETRY_INTERVAL_MS = 50;
static constexpr DWORD NF_SANDBOX_NAV_GIVEUP_MS = 10000;
#endif

static LRESULT CALLBACK NfSandboxWndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam) {
  auto handle = [&]() -> LRESULT {
    switch (msg) {
    case WM_TIMER: {
#ifdef _WIN32
      if (wParam == NF_SANDBOX_NAV_TIMER_ID) {
        auto *inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox *>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (inst)
          inst->flushPendingNavigation();
        return 0;
      }
#endif
      break;
    }
    case WM_NF_SANDBOX_FLUSH_NAV: {
#ifdef _WIN32
      auto *inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox *>(
          GetWindowLongPtrW(hWnd, GWLP_USERDATA));
      if (inst)
        inst->flushPendingNavigation();
      return 0;
#else
      break;
#endif
    }
    case WM_PAINT: {
#if !defined(NF_HAVE_WEBVIEW2) || !defined(_MSC_VER)
      PAINTSTRUCT ps{};
      HDC hdc = BeginPaint(hWnd, &ps);
      (void)hdc;
      SetBkMode(hdc, TRANSPARENT);
      RECT rc{};
      GetClientRect(hWnd, &rc);
      const wchar_t *text =
          L"NeuroForge Sandbox\n\n"
          L"This build does not include WebView2.\n"
          L"The sandbox window is a plain fallback window.\n\n"
          L"Build the vcpkg/MSVC target (with unofficial-webview2)\n"
          L"and ensure Microsoft Edge WebView2 Runtime is installed.";
      DrawTextW(hdc, text, -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);
      EndPaint(hWnd, &ps);
      auto *inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox *>(
          GetWindowLongPtrW(hWnd, GWLP_USERDATA));
      if (inst)
        inst->updateBoundsFromClient();
      return 0;
#else
      break;
#endif
    }
    case WM_SIZE: {
      auto *inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox *>(
          GetWindowLongPtrW(hWnd, GWLP_USERDATA));
      if (inst)
        inst->updateBoundsFromClient();
      break;
    }
    case WM_MOVE:
    case WM_MOVING:
    case WM_DPICHANGED: {
      auto *inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox *>(
          GetWindowLongPtrW(hWnd, GWLP_USERDATA));
      if (inst)
        inst->updateBoundsFromClient();
      break;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
  };

#if defined(_MSC_VER)
  __try {
    return handle();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    std::printf("[Sandbox] Fatal exception in WndProc (msg=0x%X)\n",
                (unsigned)msg);
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }
#else
  try {
    return handle();
  } catch (...) {
    std::printf("[Sandbox] Fatal exception in WndProc (msg=0x%X)\n",
                (unsigned)msg);
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }
#endif
}

WebSandbox::WebSandbox() {}
WebSandbox::~WebSandbox() {
#ifdef _WIN32
  HWND hwnd_for_teardown = hwnd_ ? static_cast<HWND>(hwnd_) : nullptr;
  if (hwnd_for_teardown && IsWindow(hwnd_for_teardown)) {
    KillTimer(hwnd_for_teardown, NF_SANDBOX_NAV_TIMER_ID);
    SetWindowLongPtrW(hwnd_for_teardown, GWLP_USERDATA, 0);
  }
  if (webview_nav_completed_handler_) {
    reinterpret_cast<IUnknown *>(webview_nav_completed_handler_)->Release();
    webview_nav_completed_handler_ = nullptr;
  }
  if (webview_nav_starting_handler_) {
    reinterpret_cast<IUnknown *>(webview_nav_starting_handler_)->Release();
    webview_nav_starting_handler_ = nullptr;
  }
  if (webview_process_failed_handler_) {
    reinterpret_cast<IUnknown *>(webview_process_failed_handler_)->Release();
    webview_process_failed_handler_ = nullptr;
  }
  if (webview_window_) {
    reinterpret_cast<IUnknown *>(webview_window_)->Release();
    webview_window_ = nullptr;
  }
  if (webview_controller_) {
    reinterpret_cast<IUnknown *>(webview_controller_)->Release();
    webview_controller_ = nullptr;
  }
  if (webview_env_) {
    reinterpret_cast<IUnknown *>(webview_env_)->Release();
    webview_env_ = nullptr;
  }
  hwnd_ = nullptr;
  user_data_folder_.clear();
  navigation_requested_ = false;
  navigation_completed_ = false;
  webview_process_failed_ = false;
  com_initialized_ = false;
  pending_nav_attempts_ = 0;
  pending_nav_start_tick_ = 0;
  pending_nav_last_attempt_tick_ = 0;
#endif
}

#ifdef _WIN32
static std::wstring nf_webview2_user_data_folder() {
  PWSTR known_path = nullptr;
  std::wstring root;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr,
                                     &known_path)) &&
      known_path) {
    root.assign(known_path);
    CoTaskMemFree(known_path);
    known_path = nullptr;
  }
  if (root.empty()) {
    wchar_t tmp[MAX_PATH];
    DWORD got = GetTempPathW(MAX_PATH, tmp);
    if (got > 0 && got < MAX_PATH) {
      root.assign(tmp);
    }
  }
  if (root.empty()) {
    root.assign(L".");
  }
  std::wstring folder = root;
  if (!folder.empty() && folder.back() != L'\\' && folder.back() != L'/')
    folder.push_back(L'\\');
  folder.append(L"NeuroForge\\WebView2");
  CreateDirectoryW((root + L"\\NeuroForge").c_str(), nullptr);
  CreateDirectoryW(folder.c_str(), nullptr);
  return folder;
}
#endif

bool WebSandbox::create(int width, int height, const std::string &title) {
#ifdef _WIN32
  {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
      using SetDpiCtx = BOOL(WINAPI *)(HANDLE);
      auto setCtx = reinterpret_cast<SetDpiCtx>(
          GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
      if (setCtx) {
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) - 4)
#endif
        setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
      } else {
        HMODULE shcore = GetModuleHandleW(L"Shcore.dll");
        using SetProcAwareness = HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS);
        auto setAw = reinterpret_cast<SetProcAwareness>(
            GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (setAw) {
          setAw(PROCESS_PER_MONITOR_DPI_AWARE);
        } else {
          SetProcessDPIAware();
        }
      }
    }
  }
  HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(hrCo)) {
    com_initialized_ = true;
  } else {
    std::printf("[Sandbox] CoInitializeEx(COINIT_APARTMENTTHREADED) failed: "
                "hr=0x%08X\n",
                (unsigned)hrCo);
  }
  HINSTANCE hInst = GetModuleHandle(nullptr);
  // Register a simple window class dedicated to the sandbox (use wide-char
  // APIs)
  WNDCLASSW wc{};
  wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = NfSandboxWndProc;
  wc.hInstance = hInst;
  wc.lpszClassName = L"NfSandboxWnd";
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  if (!RegisterClassW(&wc)) {
    DWORD ec = GetLastError();
    if (ec != ERROR_CLASS_ALREADY_EXISTS) {
      return false;
    }
  }
  DWORD style =
      WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  RECT r{0, 0, width, height};
  AdjustWindowRect(&r, style, FALSE);
  DWORD ex_style = 0;
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
  ex_style |= WS_EX_NOREDIRECTIONBITMAP;
#endif
  HWND hwnd =
      CreateWindowExW(ex_style, wc.lpszClassName,
                      std::wstring(title.begin(), title.end()).c_str(), style,
                      CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left,
                      r.bottom - r.top, nullptr, nullptr, hInst, nullptr);
  if (!hwnd) {
    return false;
  }
  std::printf("[Sandbox] CreateWindow hwnd=%p\n", (void *)hwnd);
  hwnd_ = hwnd;
  SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  // Cache initial client-area size and absolute screen coordinates
  RECT cr{};
  GetClientRect(hwnd, &cr);
  POINT topLeft{0, 0};
  ClientToScreen(hwnd, &topLeft);
  rect_.x = static_cast<int>(topLeft.x);
  rect_.y = static_cast<int>(topLeft.y);
  rect_.w = static_cast<int>(cr.right - cr.left);
  rect_.h = static_cast<int>(cr.bottom - cr.top);

  // Force the window to foreground/topmost and ensure it is shown
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  SetForegroundWindow(hwnd);
  SetWindowPos(hwnd, HWND_TOPMOST, 100, 100, rect_.w, rect_.h, SWP_SHOWWINDOW);
  updateBoundsFromClient();

#if !defined(NF_HAVE_WEBVIEW2)
  std::printf("[Sandbox] WebView2 is not enabled in this build; using plain "
              "window fallback.\n");
#endif

#if defined(NF_HAVE_WEBVIEW2)
  if (!SUCCEEDED(hrCo)) {
    std::printf("[Sandbox] Skipping WebView2 init (COM not initialized)\n");
    return true;
  }
  // Attempt to initialize Edge WebView2 inside the sandbox window if available
  HANDLE hReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  user_data_folder_ = nf_webview2_user_data_folder();
  std::wprintf(L"[Sandbox] WebView2 user data folder: %ls\n",
               user_data_folder_.c_str());
  {
    LPWSTR ver = nullptr;
    HRESULT hrVer = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    std::printf(
        "[Sandbox] GetAvailableCoreWebView2BrowserVersionString hr=0x%08X\n",
        (unsigned)hrVer);
    if (SUCCEEDED(hrVer) && ver) {
      std::wprintf(L"[Sandbox] WebView2 runtime version: %ls\n", ver);
      CoTaskMemFree(ver);
    }
  }
  HRESULT hrEnv = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, user_data_folder_.c_str(), nullptr,
      new ComCreationHandler<
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
          ICoreWebView2Environment>(
          IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
          [this, hwnd, hReady](
              HRESULT result, ICoreWebView2Environment *createdEnv) -> HRESULT {
            try {
              if (FAILED(result) || !createdEnv) {
                std::printf("[Sandbox] WebView2 environment "
                            "creation failed: "
                            "hr=0x%08X\n",
                            (unsigned)result);
                if (hReady)
                  SetEvent(hReady);
                return S_OK;
              }
              // Manual AddRef because we are taking
              // ownership
              createdEnv->AddRef();
              if (webview_env_) {
                reinterpret_cast<IUnknown *>(webview_env_)->Release();
              }
              webview_env_ = createdEnv;
              env_ready_ = true;
              std::printf("[Sandbox] WebView2 "
                          "environment created\n");
              HRESULT hrCtl = createdEnv->CreateCoreWebView2Controller(
                  hwnd,
                  new ComCreationHandler<
                      ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                      ICoreWebView2Controller>(
                      IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                      [this,
                       hReady](HRESULT result,
                               ICoreWebView2Controller *controller) -> HRESULT {
                        try {
                          if (FAILED(result) || !controller) {
                            std::printf("[Sandbox] WebView2 controller "
                                        "creation failed: hr=0x%08X\n",
                                        (unsigned)result);
                            if (hReady)
                              SetEvent(hReady);
                            return S_OK;
                          }
                          controller->AddRef();
                          if (webview_controller_) {
                            reinterpret_cast<IUnknown *>(webview_controller_)
                                ->Release();
                          }
                          webview_controller_ = controller;
                          controller_ready_ = true;
                          std::printf("[S"
                                      "an"
                                      "db"
                                      "ox"
                                      "] "
                                      "We"
                                      "bV"
                                      "ie"
                                      "w2"
                                      " c"
                                      "on"
                                      "tr"
                                      "ol"
                                      "le"
                                      "r "
                                      "in"
                                      "it"
                                      "ia"
                                      "li"
                                      "ze"
                                      "d"
                                      "\n");
                          ICoreWebView2 *wv = nullptr;
                          HRESULT hrWv = controller->get_CoreWebView2(&wv);
                          if (SUCCEEDED(hrWv) && wv) {
                            if (webview_window_) {
                              reinterpret_cast<IUnknown *>(webview_window_)
                                  ->Release();
                            }
                            webview_window_ = wv;
                          }
                          std::printf("[S"
                                      "an"
                                      "db"
                                      "ox"
                                      "] "
                                      "ge"
                                      "t_"
                                      "Co"
                                      "re"
                                      "We"
                                      "bV"
                                      "ie"
                                      "w2"
                                      " h"
                                      "r="
                                      "0x"
                                      "%0"
                                      "8X"
                                      " w"
                                      "v="
                                      "%p"
                                      "\n",
                                      (unsigned)hrWv, (void *)wv);

                          // No
                          // WRL
                          // usage
                          // for
                          // QueryInterface
                          ICoreWebView2Controller2 *ctrl2 = nullptr;
                          if (SUCCEEDED(controller->QueryInterface(
                                  IID_ICoreWebView2Controller2,
                                  (void **)&ctrl2)) &&
                              ctrl2) {
                            COREWEBVIEW2_COLOR
                            bg{};
                            bg.A = 255;
                            bg.R = 255;
                            bg.G = 255;
                            bg.B = 255;
                            ctrl2->put_DefaultBackgroundColor(bg);
                            ctrl2->Release();
                          }
                          if (hwnd_) {
                            RECT rc{};
                            GetClientRect(static_cast<HWND>(hwnd_), &rc);
                            HRESULT hrB = controller->put_Bounds(rc);
                            HRESULT hrV = controller->put_IsVisible(TRUE);
                            std::printf("[Sandbox] put_Bounds hr=0x%08X "
                                        "rc=%ld,%ld,%ld,%ld\n",
                                        (unsigned)hrB, rc.left, rc.top,
                                        rc.right, rc.bottom);
                            std::printf("[Sandbox] put_IsVisible hr=0x%08X\n",
                                        (unsigned)hrV);
                            controller->NotifyParentWindowPositionChanged();
                            EnumChildWindows(
                                static_cast<HWND>(hwnd_),
                                [](HWND child, LPARAM) -> BOOL {
                                  wchar_t cls[128]{};
                                  GetClassNameW(
                                      child, cls,
                                      static_cast<int>(sizeof(cls) /
                                                       sizeof(cls[0])));
                                  RECT r{};
                                  GetWindowRect(child, &r);
                                  std::wprintf(
                                      L"[Sandbox] child hwnd=%p class=%ls "
                                      L"rect=%ld,%ld,%ld,%ld\n",
                                      (void *)child, cls, r.left, r.top,
                                      r.right, r.bottom);
                                  return TRUE;
                                },
                                0);
                          }
                          if (webview_window_) {
                            auto *wv2 =
                                static_cast<ICoreWebView2 *>(webview_window_);
                            auto store_handler = [this](void *&slot,
                                                        IUnknown *p) {
                              if (slot) {
                                reinterpret_cast<IUnknown *>(slot)->Release();
                                slot = nullptr;
                              }
                              // We do NOT AddRef here because ComEventHandler
                              // starts with RefCount=1 and we are storing that
                              // initial reference. However, if 'p' came from
                              // elsewhere we might need to check. here 'p' is
                              // 'h'. 'h' is 'new ComEventHandler'. It has
                              // refcount 1. We store it.
                              slot = p;
                            };
                            {
                              EventRegistrationToken tok{};
                              auto *h = new ComEventHandler<
                                  ICoreWebView2NavigationCompletedEventHandler,
                                  ICoreWebView2NavigationCompletedEventArgs>(
                                  IID_ICoreWebView2NavigationCompletedEventHandler,
                                  [this](
                                      ICoreWebView2 *sender,
                                      ICoreWebView2NavigationCompletedEventArgs
                                          *args) -> HRESULT {
                                    BOOL ok = FALSE;
                                    if (args)
                                      args->get_IsSuccess(&ok);
                                    COREWEBVIEW2_WEB_ERROR_STATUS
                                    st = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                                    if (args)
                                      args->get_WebErrorStatus(&st);
                                    std::printf("[Sandbox] NavigationCompleted "
                                                "success=%d web_error=%d\n",
                                                ok ? 1 : 0, (int)st);
                                    navigation_completed_ = true;
                                    (void)sender;
                                    return S_OK;
                                  });
                              store_handler(webview_nav_completed_handler_, h);
                              HRESULT hrNav = wv2->add_NavigationCompleted(
                                  static_cast<
                                      ICoreWebView2NavigationCompletedEventHandler
                                          *>(h),
                                  &tok);
                              std::printf("[Sandbox] add_NavigationCompleted "
                                          "hr=0x%08X\n",
                                          (unsigned)hrNav);
                            }
                            {
                              EventRegistrationToken tok{};
                              auto *h = new ComEventHandler<
                                  ICoreWebView2NavigationStartingEventHandler,
                                  ICoreWebView2NavigationStartingEventArgs>(
                                  IID_ICoreWebView2NavigationStartingEventHandler,
                                  [this](
                                      ICoreWebView2 *sender,
                                      ICoreWebView2NavigationStartingEventArgs
                                          *args) -> HRESULT {
                                    navigation_started_ = true;
                                    navigation_completed_ = false;
                                    LPWSTR uri = nullptr;
                                    if (args)
                                      args->get_Uri(&uri);
                                    if (uri) {
                                      std::wprintf(
                                          L"[Sandbox] NavigationStarting "
                                          L"uri=%ls\n",
                                          uri);
                                      CoTaskMemFree(uri);
                                    } else {
                                      std::printf(
                                          "[Sandbox] NavigationStarting\n");
                                    }
                                    (void)sender;
                                    return S_OK;
                                  });
                              store_handler(webview_nav_starting_handler_, h);
                              HRESULT hrNavS = wv2->add_NavigationStarting(
                                  static_cast<
                                      ICoreWebView2NavigationStartingEventHandler
                                          *>(h),
                                  &tok);
                              std::printf("[Sandbox] add_NavigationStarting "
                                          "hr=0x%08X\n",
                                          (unsigned)hrNavS);
                            }
                            {
                              EventRegistrationToken tok{};
                              auto *h = new ComEventHandler<
                                  ICoreWebView2ProcessFailedEventHandler,
                                  ICoreWebView2ProcessFailedEventArgs>(
                                  IID_ICoreWebView2ProcessFailedEventHandler,
                                  [this](
                                      ICoreWebView2 *sender,
                                      ICoreWebView2ProcessFailedEventArgs *args)
                                      -> HRESULT {
                                    COREWEBVIEW2_PROCESS_FAILED_KIND
                                    kind =
                                        COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
                                    if (args)
                                      args->get_ProcessFailedKind(&kind);
                                    std::printf(
                                        "[Sandbox] ProcessFailed kind=%d\n",
                                        (int)kind);
                                    webview_process_failed_ = true;
                                    (void)sender;
                                    return S_OK;
                                  });
                              store_handler(webview_process_failed_handler_, h);
                              HRESULT hrProc = wv2->add_ProcessFailed(
                                  static_cast<
                                      ICoreWebView2ProcessFailedEventHandler *>(
                                      h),
                                  &tok);
                              std::printf(
                                  "[Sandbox] add_ProcessFailed hr=0x%08X\n",
                                  (unsigned)hrProc);
                            }
                            if (hwnd_) {
                              PostMessageW(static_cast<HWND>(hwnd_),
                                           WM_NF_SANDBOX_FLUSH_NAV, 0, 0);
                              SetTimer(static_cast<HWND>(hwnd_),
                                       NF_SANDBOX_NAV_TIMER_ID,
                                       NF_SANDBOX_NAV_RETRY_INTERVAL_MS,
                                       nullptr);
                            }
                          }
                          std::printf("[S"
                                      "an"
                                      "db"
                                      "ox"
                                      "] "
                                      "We"
                                      "bV"
                                      "ie"
                                      "w2"
                                      " a"
                                      "tt"
                                      "ac"
                                      "he"
                                      "d "
                                      "to"
                                      " s"
                                      "an"
                                      "db"
                                      "ox"
                                      " "
                                      "wi"
                                      "nd"
                                      "ow"
                                      "\n");
                          if (hReady)
                            SetEvent(hReady);
                          return S_OK;
                        } catch (...) {
                          std::printf("[S"
                                      "an"
                                      "db"
                                      "ox"
                                      "] "
                                      "Fa"
                                      "ta"
                                      "l "
                                      "C+"
                                      "+ "
                                      "ex"
                                      "ce"
                                      "pt"
                                      "io"
                                      "n "
                                      "in"
                                      " "
                                      "We"
                                      "bV"
                                      "ie"
                                      "w2"
                                      " c"
                                      "on"
                                      "tr"
                                      "ol"
                                      "le"
                                      "r "
                                      "ca"
                                      "ll"
                                      "ba"
                                      "ck"
                                      "\n");
                          if (hReady)
                            SetEvent(hReady);
                          return S_OK;
                        }
                      }));
              std::printf("[Sandbox] "
                          "CreateCoreWebView2Controll"
                          "er returned hr=0x%08X\n",
                          (unsigned)hrCtl);
              if (FAILED(hrCtl) && hReady)
                SetEvent(hReady);
              return S_OK;
            } catch (...) {
              std::printf("[Sandbox] Fatal C++ "
                          "exception in WebView2 "
                          "environment callback\n");
              if (hReady)
                SetEvent(hReady);
              return S_OK;
            }
          }));
  // Wait briefly for controller creation to complete so first navigation occurs
  // immediately
  if (hReady) {
    DWORD start = GetTickCount();
    MSG msg;
    while (WaitForSingleObject(hReady, 0) == WAIT_TIMEOUT) {
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      if (GetTickCount() - start > 5000) {
        std::printf("[Sandbox] Timeout waiting for WebView2 controller\n");
        break;
      }
      Sleep(10);
    }
    CloseHandle(hReady);
  }
  (void)hrEnv;
#endif
  return true;
#else
  (void)width;
  (void)height;
  (void)title;
  return false;
#endif
}

void WebSandbox::flushPendingNavigation() {
#ifdef _WIN32
#if defined(NF_HAVE_WEBVIEW2)
  if (pending_url_.empty()) {
    if (hwnd_) {
      KillTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID);
    }
    return;
  }
  if (!webview_window_) {
    if (hwnd_) {
      SetTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID,
               NF_SANDBOX_NAV_RETRY_INTERVAL_MS, nullptr);
    }
    return;
  }
  DWORD now = GetTickCount();
  if (pending_nav_last_attempt_tick_ != 0) {
    DWORD since_last = now - static_cast<DWORD>(pending_nav_last_attempt_tick_);
    if (since_last < NF_SANDBOX_NAV_RETRY_INTERVAL_MS) {
      return;
    }
  }
  pending_nav_last_attempt_tick_ = static_cast<std::uint32_t>(now);
  if (pending_nav_start_tick_ == 0) {
    pending_nav_start_tick_ = static_cast<std::uint32_t>(now);
    pending_nav_attempts_ = 0;
  }
  DWORD elapsed = now - static_cast<DWORD>(pending_nav_start_tick_);
  if (elapsed >= NF_SANDBOX_NAV_GIVEUP_MS) {
    std::printf(
        "[Sandbox] Pending navigation gave up after %lu ms attempts=%d\n",
        (unsigned long)elapsed, pending_nav_attempts_);
    webview_process_failed_ = true;
    pending_url_.clear();
    if (hwnd_) {
      KillTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID);
    }
    pending_nav_attempts_ = 0;
    pending_nav_start_tick_ = 0;
    pending_nav_last_attempt_tick_ = 0;
    return;
  }

  auto *wv = static_cast<ICoreWebView2 *>(webview_window_);
  std::wstring wurl(pending_url_.begin(), pending_url_.end());
  std::wprintf(L"[Sandbox] Navigating to URL: %ls\n", wurl.c_str());
  navigation_completed_ = false;
  webview_process_failed_ = false;
  HRESULT hrNav = wv->Navigate(wurl.c_str());
  std::printf("[Sandbox] Navigate hr=0x%08X\n", (unsigned)hrNav);
  pending_nav_attempts_ += 1;
  if (SUCCEEDED(hrNav)) {
    navigation_started_ = true;
    pending_url_.clear();
    if (hwnd_) {
      KillTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID);
    }
    pending_nav_attempts_ = 0;
    pending_nav_start_tick_ = 0;
    pending_nav_last_attempt_tick_ = 0;
    return;
  }
  if (hwnd_) {
    SetTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID,
             NF_SANDBOX_NAV_RETRY_INTERVAL_MS, nullptr);
  }
#else
  if (hwnd_) {
    KillTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID);
  }
  pending_url_.clear();
#endif
#else
  pending_url_.clear();
#endif
}

bool WebSandbox::navigate(const std::string &url) {
#ifdef _WIN32
#if defined(NF_HAVE_WEBVIEW2)
  // Sandbox is observation-only; no sensory data is routed to learning at Stage
  // C v1.
  navigation_requested_ = true;
  pending_url_ = url;
  navigation_completed_ = false;
  webview_process_failed_ = false;
  pending_nav_start_tick_ = 0;
  pending_nav_attempts_ = 0;
  pending_nav_last_attempt_tick_ = 0;
  if (hwnd_) {
    PostMessageW(static_cast<HWND>(hwnd_), WM_NF_SANDBOX_FLUSH_NAV, 0, 0);
    SetTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID,
             NF_SANDBOX_NAV_RETRY_INTERVAL_MS, nullptr);
  }
#endif
  return true;
#else
  (void)url;
  return false;
#endif
}

bool WebSandbox::scroll(int delta) {
#ifdef _WIN32
  // Send a simple mouse wheel event to the sandbox window client area
  if (!hwnd_)
    return false;
  HWND hwnd = static_cast<HWND>(hwnd_);
  POINT pt{};
  GetCursorPos(&pt);
  ScreenToClient(hwnd, &pt);
  SendMessage(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, delta),
              MAKELPARAM(pt.x, pt.y));
  return true;
#else
  (void)delta;
  return false;
#endif
}

bool WebSandbox::click(int cx, int cy) {
#ifdef _WIN32
  // Send a left-click at client coordinates (cx, cy)
  if (!hwnd_)
    return false;
  HWND hwnd = static_cast<HWND>(hwnd_);
  LPARAM pos = MAKELPARAM(cx, cy);
  SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, pos);
  SendMessage(hwnd, WM_LBUTTONUP, 0, pos);
  return true;
#else
  (void)cx;
  (void)cy;
  return false;
#endif
}

bool WebSandbox::typeText(const std::string &text) {
#ifdef _WIN32
  if (!hwnd_)
    return false;
  HWND hwnd = static_cast<HWND>(hwnd_);
  for (char ch : text) {
    SendMessage(hwnd, WM_CHAR,
                static_cast<WPARAM>(static_cast<unsigned char>(ch)), 0);
  }
  return true;
#else
  (void)text;
  return false;
#endif
}

bool WebSandbox::focus() {
#ifdef _WIN32
  if (!hwnd_)
    return false;
  HWND hwnd = static_cast<HWND>(hwnd_);
  SetForegroundWindow(hwnd);
  SetFocus(hwnd);
  return true;
#else
  return false;
#endif
}

bool WebSandbox::sendKey(unsigned int vk) {
#ifdef _WIN32
  if (!hwnd_)
    return false;
  HWND hwnd = static_cast<HWND>(hwnd_);
  SendMessage(hwnd, WM_KEYDOWN, static_cast<WPARAM>(vk), 0);
  SendMessage(hwnd, WM_KEYUP, static_cast<WPARAM>(vk), 0);
  return true;
#else
  (void)vk;
  return false;
#endif
}

::NeuroForge::Sandbox::SandboxRect WebSandbox::bounds() const {
  ::NeuroForge::Sandbox::SandboxRect r{0, 0, rect_.w, rect_.h};
  return r;
}
::NeuroForge::Sandbox::SandboxRect WebSandbox::screenBounds() const {
#ifdef _WIN32
  if (!hwnd_)
    return rect_;
  HWND hwnd = static_cast<HWND>(hwnd_);
  RECT cr{};
  GetClientRect(hwnd, &cr);
  POINT tl{0, 0};
  ClientToScreen(hwnd, &tl);
  ::NeuroForge::Sandbox::SandboxRect r{};
  r.x = static_cast<int>(tl.x);
  r.y = static_cast<int>(tl.y);
  r.w = static_cast<int>(cr.right - cr.left);
  r.h = static_cast<int>(cr.bottom - cr.top);
  return r;
#else
  return rect_;
#endif
}
bool WebSandbox::isOpen() const { return hwnd_ != nullptr; }

#ifdef _WIN32
bool WebSandbox::waitUntilReady(int timeout_ms) {
  DWORD start = GetTickCount();
  MSG msg;
  for (;;) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    flushPendingNavigation();
    bool ready = (env_ready_ && controller_ready_ && bounds_update_count_ > 0);
    if (ready && navigation_requested_) {
      ready = pending_url_.empty() || navigation_started_;
    }
    if (ready)
      return true;
    DWORD elapsed = GetTickCount() - start;
    if (elapsed >= static_cast<DWORD>(timeout_ms))
      break;
    Sleep(10);
  }
  std::printf(
      "[Sandbox] waitUntilReady timeout: env=%d controller=%d "
      "bounds_updates=%d nav_started=%d nav_completed=%d proc_failed=%d\n",
      env_ready_ ? 1 : 0, controller_ready_ ? 1 : 0, bounds_update_count_,
      navigation_started_ ? 1 : 0, navigation_completed_ ? 1 : 0,
      webview_process_failed_ ? 1 : 0);
  return false;
}
#else
bool WebSandbox::waitUntilReady(int timeout_ms) {
  (void)timeout_ms;
  return false;
}
#endif

static std::string WideToUtf8(const std::wstring &wstr) {
  if (wstr.empty())
    return {};
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
}

std::string WebSandbox::executeScriptBlocking(const std::wstring &script) {
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
  if (!webview_window_)
    return "{}";

  auto *wv = static_cast<ICoreWebView2 *>(webview_window_);
  std::string result_json;
  bool completed = false;

  HRESULT hr = wv->ExecuteScript(
      script.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                          [&](HRESULT error, LPCWSTR result) -> HRESULT {
                            if (SUCCEEDED(error) && result) {
                              result_json = WideToUtf8(result);
                            }
                            completed = true;
                            return S_OK;
                          })
                          .Get());

  if (FAILED(hr))
    return "{}";

  DWORD start = GetTickCount();
  MSG msg;
  while (!completed) {
    if (GetTickCount() - start > 5000)
      break;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (completed)
        break;
    }
    if (!completed)
      WaitMessage();
  }
  return result_json;
#else
  return "{}";
#endif
}

bool WebSandbox::scrollToElement(const std::string &selector) {
  std::string s =
      "var el = document.querySelector('" + selector +
      "'); if(el) el.scrollIntoView({behavior: 'smooth', block: 'center'});";
  std::wstring ws(s.begin(), s.end());
  executeScriptBlocking(ws);
  return true;
}

std::pair<int, int> WebSandbox::getScrollPosition() {
  std::string json = executeScriptBlocking(
      L"JSON.stringify({x: window.scrollX, y: window.scrollY})");
  int x = 0, y = 0;
  if (json.empty() || json == "null")
    return {0, 0};
  // Simple parse: {"x":0,"y":100}
  auto getVal = [&](const std::string &key) -> int {
    size_t pos = json.find("\"" + key + "\":");
    if (pos == std::string::npos)
      return 0;
    return std::atoi(json.c_str() + pos + key.length() + 3); // Skip ":
  };
  return {getVal("x"), getVal("y")};
}

SandboxRect WebSandbox::getElementBounds(const std::string &selector) {
  std::string s =
      "var el = document.querySelector('" + selector +
      "');"
      "var r = el ? el.getBoundingClientRect() : {x:0,y:0,width:0,height:0};"
      "JSON.stringify({x: Math.round(r.x), y: Math.round(r.y), w: "
      "Math.round(r.width), h: Math.round(r.height)})";
  std::wstring ws(s.begin(), s.end());
  std::string json = executeScriptBlocking(ws);

  auto getVal = [&](const std::string &key) -> int {
    size_t pos = json.find("\"" + key + "\":");
    if (pos == std::string::npos)
      return 0;
    return std::atoi(json.c_str() + pos + key.length() + 3);
  };

  SandboxRect r;
  r.x = getVal("x");
  r.y = getVal("y");
  r.w = getVal("w");
  r.h = getVal("h");
  return r;
}

bool WebSandbox::clickElement(const std::string &selector) {
  SandboxRect r = getElementBounds(selector);
  if (r.w == 0 && r.h == 0)
    return false;

  int cx = r.x + r.w / 2;
  int cy = r.y + r.h / 2;
  return click(cx, cy);
}

ScreenshotInfo WebSandbox::captureScreenshot(const ScreenshotConfig &config) {
  screenshot_capture_.setConfig(config);
  // getCurrentUrl may need to be called if not up to date, but we assume it's
  // reasonably current or updated elsewhere. Actually, let's just pass what we
  // have.
  return screenshot_capture_.captureViewport(hwnd_, getCurrentUrl());
}

#ifdef _WIN32
void WebSandbox::updateBoundsFromClient() {
  if (!hwnd_)
    return;
  HWND hwnd = static_cast<HWND>(hwnd_);
#if defined(NF_HAVE_WEBVIEW2)
  if (webview_controller_) {
    auto *ctrl = static_cast<ICoreWebView2Controller *>(webview_controller_);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    // Standard COM call, no SEH needed for pure COM interface on MinGW
    // If it crashes, it's a fatal process state anyway.
    HRESULT hr = ctrl->put_Bounds(rc);
    if (FAILED(hr)) {
      // Log or handle error if needed
      // webview_controller_ = nullptr; // Don't nullify on simple failure
    } else {
      ctrl->NotifyParentWindowPositionChanged();
    }
  }
#endif
  RECT cr{};
  GetClientRect(hwnd, &cr);
  rect_.w = static_cast<int>(cr.right - cr.left);
  rect_.h = static_cast<int>(cr.bottom - cr.top);
  bounds_update_count_ += 1;
}

void WebSandbox::poll() {
#ifdef _WIN32
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  flushPendingNavigation();
#endif
}

std::string WebSandbox::getPageText() const { return extracted_text_; }

std::string WebSandbox::getPageTitle() const { return page_title_; }

std::string WebSandbox::getCurrentUrl() const { return current_url_; }

bool WebSandbox::isExtractionComplete() const { return extraction_complete_; }

bool WebSandbox::extractPageContent() {
#if defined(NF_HAVE_WEBVIEW2)
  if (!webview_window_)
    return false;

  extraction_complete_ = false;
  extraction_in_progress_ = true;
  extracted_text_.clear();
  page_title_.clear();

  auto wideToUtf8 = [](const wchar_t *w) -> std::string {
    if (!w)
      return {};
    int len =
        WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
      return {};
    std::string out;
    out.resize(static_cast<std::size_t>(len));
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0')
      out.pop_back();
    return out;
  };

  auto unescapeJsonStringLiteral = [](const std::string &in) -> std::string {
    std::string s = in;
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
      s = s.substr(1, s.size() - 2);
    }
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
      char c = s[i];
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (i + 1 >= s.size())
        break;
      char esc = s[++i];
      switch (esc) {
      case '"':
        out.push_back('"');
        break;
      case '\\':
        out.push_back('\\');
        break;
      case '/':
        out.push_back('/');
        break;
      case 'b':
        out.push_back('\b');
        break;
      case 'f':
        out.push_back('\f');
        break;
      case 'n':
        out.push_back('\n');
        break;
      case 'r':
        out.push_back('\r');
        break;
      case 't':
        out.push_back('\t');
        break;
      case 'u': {
        if (i + 4 < s.size()) {
          auto hex = [&](char h) -> int {
            if (h >= '0' && h <= '9')
              return h - '0';
            if (h >= 'a' && h <= 'f')
              return 10 + (h - 'a');
            if (h >= 'A' && h <= 'F')
              return 10 + (h - 'A');
            return -1;
          };
          int h1 = hex(s[i + 1]);
          int h2 = hex(s[i + 2]);
          int h3 = hex(s[i + 3]);
          int h4 = hex(s[i + 4]);
          if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
            unsigned code =
                static_cast<unsigned>((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
            if (code <= 0x7F) {
              out.push_back(static_cast<char>(code));
            } else if (code <= 0x7FF) {
              out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
              out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
              out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            i += 4;
          }
        }
        break;
      }
      default:
        out.push_back(esc);
        break;
      }
    }
    return out;
  };

  // Get current URL from WebView2
  ICoreWebView2 *wv = reinterpret_cast<ICoreWebView2 *>(webview_window_);
  LPWSTR source_url = nullptr;
  HRESULT hr = wv->get_Source(&source_url);

  if (SUCCEEDED(hr) && source_url) {
    current_url_ = wideToUtf8(source_url);
    CoTaskMemFree(source_url);
  } else {
    current_url_.clear();
  }

  // Get document title (best-effort)
  {
    LPWSTR wtitle = nullptr;
    if (SUCCEEDED(wv->get_DocumentTitle(&wtitle)) && wtitle) {
      page_title_ = wideToUtf8(wtitle);
      CoTaskMemFree(wtitle);
    }
  }

  const wchar_t *script = LR"JS((function(){
        function pickText(){
          var selectors = ['main','article','.mw-parser-output','#content','#mw-content-text'];
          for (var i=0;i<selectors.length;i++){
            try{
              var el = document.querySelector(selectors[i]);
              if (el && el.innerText && el.innerText.length > 200) return el.innerText;
            } catch(e){}
          }
          try{
            if (document.body && document.body.innerText) return document.body.innerText;
          } catch(e){}
          return '';
        }
        var t = pickText();
        return t;
      })();)JS";

  auto *handler =
      new ComCreationHandler<ICoreWebView2ExecuteScriptCompletedHandler,
                             const wchar_t>(
          IID_ICoreWebView2ExecuteScriptCompletedHandler,
          [this, wideToUtf8, unescapeJsonStringLiteral](
              HRESULT error, const wchar_t *resultObjectAsJson) -> HRESULT {
            if (FAILED(error) || !resultObjectAsJson) {
              extraction_in_progress_ = false;
              extraction_complete_ = true;
              extracted_text_.clear();
              return S_OK;
            }
            std::string json = wideToUtf8(resultObjectAsJson);
            extracted_text_ = unescapeJsonStringLiteral(json);
            extraction_in_progress_ = false;
            extraction_complete_ = true;
            return S_OK;
          });
  HRESULT hrExec = wv->ExecuteScript(script, handler);
  handler->Release(); // Pass ownership to WebView2

  if (FAILED(hrExec)) {
    extraction_in_progress_ = false;
    extraction_complete_ = true;
    extracted_text_.clear();
    return false;
  }

  return true;
#else
  // No WebView2 - extraction not supported
  return false;
#endif
}

#else
void WebSandbox::updateBoundsFromClient() {}
void WebSandbox::poll() {}
std::string WebSandbox::getPageText() const { return ""; }
std::string WebSandbox::getPageTitle() const { return ""; }
std::string WebSandbox::getCurrentUrl() const { return ""; }
bool WebSandbox::isExtractionComplete() const { return false; }
bool WebSandbox::extractPageContent() { return false; }
bool WebSandbox::scrollToElement(const std::string &selector) { return false; }
std::pair<int, int> WebSandbox::getScrollPosition() { return {0, 0}; }
SandboxRect WebSandbox::getElementBounds(const std::string &selector) {
  return {0, 0, 0, 0};
}
bool WebSandbox::clickElement(const std::string &selector) { return false; }
std::string WebSandbox::executeScriptBlocking(const std::wstring &script) {
  return "{}";
}

ScreenshotInfo WebSandbox::captureScreenshot(const ScreenshotConfig &config) {
  (void)config;
  return {};
}
#endif

} // namespace Sandbox
} // namespace NeuroForge
