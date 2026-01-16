#include "sandbox/WebSandbox.h"
#include <cstdio> // debug logging for sandbox events

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <shellscalingapi.h>
#include <objbase.h>
#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "Shell32.lib")
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
#include <WebView2.h>
#include <wrl.h>
using Microsoft::WRL::Callback;
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

#ifdef _WIN32
static LRESULT CALLBACK NfSandboxWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    __try {
        switch(msg){
            case WM_TIMER: {
                if (wParam == NF_SANDBOX_NAV_TIMER_ID) {
                    auto* inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                    if (inst) inst->flushPendingNavigation();
                    return 0;
                }
                break;
            }
            case WM_NF_SANDBOX_FLUSH_NAV: {
#ifdef _WIN32
                auto* inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                if (inst) inst->flushPendingNavigation();
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
                const wchar_t* text =
                    L"NeuroForge Sandbox\n\n"
                    L"This build does not include WebView2.\n"
                    L"The sandbox window is a plain fallback window.\n\n"
                    L"Build the vcpkg/MSVC target (with unofficial-webview2)\n"
                    L"and ensure Microsoft Edge WebView2 Runtime is installed.";
                DrawTextW(hdc, text, -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);
                EndPaint(hWnd, &ps);
                auto* inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                if (inst) inst->updateBoundsFromClient();
                return 0;
#else
                break;
#endif
            }
            case WM_SIZE: {
                auto* inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                if (inst) inst->updateBoundsFromClient();
                break;
            }
            case WM_MOVE:
            case WM_MOVING:
            case WM_DPICHANGED: {
                auto* inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                if (inst) inst->updateBoundsFromClient();
                break;
            }
            case WM_DESTROY: PostQuitMessage(0); return 0;
            default: return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        std::printf("[Sandbox] Fatal exception in WndProc (msg=0x%X)\n", (unsigned)msg);
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}
#endif

WebSandbox::WebSandbox() {}
WebSandbox::~WebSandbox() {
#ifdef _WIN32
    HWND hwnd_for_teardown = hwnd_ ? static_cast<HWND>(hwnd_) : nullptr;
    if (hwnd_for_teardown && IsWindow(hwnd_for_teardown)) {
        KillTimer(hwnd_for_teardown, NF_SANDBOX_NAV_TIMER_ID);
        SetWindowLongPtrW(hwnd_for_teardown, GWLP_USERDATA, 0);
    }
    if (webview_nav_completed_handler_) {
        reinterpret_cast<IUnknown*>(webview_nav_completed_handler_)->Release();
        webview_nav_completed_handler_ = nullptr;
    }
    if (webview_nav_starting_handler_) {
        reinterpret_cast<IUnknown*>(webview_nav_starting_handler_)->Release();
        webview_nav_starting_handler_ = nullptr;
    }
    if (webview_process_failed_handler_) {
        reinterpret_cast<IUnknown*>(webview_process_failed_handler_)->Release();
        webview_process_failed_handler_ = nullptr;
    }
    if (webview_window_) {
        reinterpret_cast<IUnknown*>(webview_window_)->Release();
        webview_window_ = nullptr;
    }
    if (webview_controller_) {
        reinterpret_cast<IUnknown*>(webview_controller_)->Release();
        webview_controller_ = nullptr;
    }
    if (webview_env_) {
        reinterpret_cast<IUnknown*>(webview_env_)->Release();
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
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known_path)) && known_path) {
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
    if (!folder.empty() && folder.back() != L'\\' && folder.back() != L'/') folder.push_back(L'\\');
    folder.append(L"NeuroForge\\WebView2");
    CreateDirectoryW((root + L"\\NeuroForge").c_str(), nullptr);
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder;
}
#endif

bool WebSandbox::create(int width, int height, const std::string& title){
#ifdef _WIN32
    {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            using SetDpiCtx = BOOL (WINAPI *)(HANDLE);
            auto setCtx = reinterpret_cast<SetDpiCtx>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
            if (setCtx) {
                #ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
                #define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
                #endif
                setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            } else {
                HMODULE shcore = GetModuleHandleW(L"Shcore.dll");
                using SetProcAwareness = HRESULT (WINAPI *)(PROCESS_DPI_AWARENESS);
                auto setAw = reinterpret_cast<SetProcAwareness>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
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
        std::printf("[Sandbox] CoInitializeEx(COINIT_APARTMENTTHREADED) failed: hr=0x%08X\n", (unsigned)hrCo);
    }
    HINSTANCE hInst = GetModuleHandle(nullptr);
    // Register a simple window class dedicated to the sandbox (use wide-char APIs)
    WNDCLASSW wc{}; wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW; wc.lpfnWndProc = NfSandboxWndProc; wc.hInstance = hInst; wc.lpszClassName = L"NfSandboxWnd"; wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if(!RegisterClassW(&wc)){
        DWORD ec = GetLastError();
        if (ec != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    RECT r{0,0,width,height}; AdjustWindowRect(&r, style, FALSE);
    DWORD ex_style = 0;
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
    ex_style |= WS_EX_NOREDIRECTIONBITMAP;
#endif
    HWND hwnd = CreateWindowExW(ex_style, wc.lpszClassName, std::wstring(title.begin(), title.end()).c_str(), style,
                               CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, hInst, nullptr);
    if(!hwnd){
        return false;
    }
    std::printf("[Sandbox] CreateWindow hwnd=%p\n", (void*)hwnd);
    hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    // Cache initial client-area size and absolute screen coordinates
    RECT cr{}; GetClientRect(hwnd, &cr);
    POINT topLeft{0,0}; ClientToScreen(hwnd, &topLeft);
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

#if !defined(NF_HAVE_WEBVIEW2) || !defined(_MSC_VER)
    std::printf("[Sandbox] WebView2 is not enabled in this build; using plain window fallback.\n");
#endif

#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
    if (!SUCCEEDED(hrCo)) {
        std::printf("[Sandbox] Skipping WebView2 init (COM not initialized)\n");
        return true;
    }
    // Attempt to initialize Edge WebView2 inside the sandbox window if available
    HANDLE hReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    user_data_folder_ = nf_webview2_user_data_folder();
    std::wprintf(L"[Sandbox] WebView2 user data folder: %ls\n", user_data_folder_.c_str());
    {
        LPWSTR ver = nullptr;
        HRESULT hrVer = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
        std::printf("[Sandbox] GetAvailableCoreWebView2BrowserVersionString hr=0x%08X\n", (unsigned)hrVer);
        if (SUCCEEDED(hrVer) && ver) {
            std::wprintf(L"[Sandbox] WebView2 runtime version: %ls\n", ver);
            CoTaskMemFree(ver);
        }
    }
    HRESULT hrEnv = CreateCoreWebView2EnvironmentWithOptions(nullptr, user_data_folder_.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hwnd, hReady](HRESULT result, ICoreWebView2Environment* createdEnv) -> HRESULT {
                try {
                    if (FAILED(result) || !createdEnv) {
                        std::printf("[Sandbox] WebView2 environment creation failed: hr=0x%08X\n", (unsigned)result);
                        if (hReady) SetEvent(hReady);
                        return S_OK;
                    }
                    createdEnv->AddRef();
                    if (webview_env_) {
                        reinterpret_cast<IUnknown*>(webview_env_)->Release();
                    }
                    webview_env_ = createdEnv;
                    env_ready_ = true;
                    std::printf("[Sandbox] WebView2 environment created\n");
                    HRESULT hrCtl = createdEnv->CreateCoreWebView2Controller(hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this, hReady](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                try {
                                    if (FAILED(result) || !controller) {
                                        std::printf("[Sandbox] WebView2 controller creation failed: hr=0x%08X\n", (unsigned)result);
                                        if (hReady) SetEvent(hReady);
                                        return S_OK;
                                    }
                                    controller->AddRef();
                                    if (webview_controller_) {
                                        reinterpret_cast<IUnknown*>(webview_controller_)->Release();
                                    }
                                    webview_controller_ = controller;
                                    controller_ready_ = true;
                                    std::printf("[Sandbox] WebView2 controller initialized\n");
                                    ICoreWebView2* wv = nullptr;
                                    HRESULT hrWv = controller->get_CoreWebView2(&wv);
                                    if (SUCCEEDED(hrWv) && wv) {
                                        if (webview_window_) {
                                            reinterpret_cast<IUnknown*>(webview_window_)->Release();
                                        }
                                        webview_window_ = wv;
                                    }
                                    std::printf("[Sandbox] get_CoreWebView2 hr=0x%08X wv=%p\n", (unsigned)hrWv, (void*)wv);
                                    ICoreWebView2Controller2* ctrl2 = nullptr;
                                    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl2))) && ctrl2) {
                                        COREWEBVIEW2_COLOR bg{}; bg.A = 255; bg.R = 255; bg.G = 255; bg.B = 255;
                                        ctrl2->put_DefaultBackgroundColor(bg);
                                        ctrl2->Release();
                                    }
                                    if (hwnd_) {
                                        RECT rc{}; GetClientRect(static_cast<HWND>(hwnd_), &rc);
                                        HRESULT hrB = controller->put_Bounds(rc);
                                        HRESULT hrV = controller->put_IsVisible(TRUE);
                                        std::printf("[Sandbox] put_Bounds hr=0x%08X rc=%ld,%ld,%ld,%ld\n", (unsigned)hrB, rc.left, rc.top, rc.right, rc.bottom);
                                        std::printf("[Sandbox] put_IsVisible hr=0x%08X\n", (unsigned)hrV);
                                        controller->NotifyParentWindowPositionChanged();
                                        EnumChildWindows(static_cast<HWND>(hwnd_),
                                                         [](HWND child, LPARAM) -> BOOL {
                                                             wchar_t cls[128]{};
                                                             GetClassNameW(child, cls, static_cast<int>(sizeof(cls) / sizeof(cls[0])));
                                                             RECT r{};
                                                             GetWindowRect(child, &r);
                                                             std::wprintf(L"[Sandbox] child hwnd=%p class=%ls rect=%ld,%ld,%ld,%ld\n",
                                                                          (void*)child, cls, r.left, r.top, r.right, r.bottom);
                                                             return TRUE;
                                                         },
                                                         0);
                                    }
                                    if (webview_window_) {
                                        auto* wv2 = static_cast<ICoreWebView2*>(webview_window_);
                                        auto store_handler = [this](void*& slot, IUnknown* p) {
                                            if (slot) {
                                                reinterpret_cast<IUnknown*>(slot)->Release();
                                                slot = nullptr;
                                            }
                                            if (p) {
                                                p->AddRef();
                                                slot = p;
                                            }
                                        };
                                        {
                                            EventRegistrationToken tok{};
                                            auto h = Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                                [this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                                    BOOL ok = FALSE;
                                                    if (args) args->get_IsSuccess(&ok);
                                                    COREWEBVIEW2_WEB_ERROR_STATUS st = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                                                    if (args) args->get_WebErrorStatus(&st);
                                                    std::printf("[Sandbox] NavigationCompleted success=%d web_error=%d\n", ok ? 1 : 0, (int)st);
                                                    navigation_completed_ = true;
                                                    (void)sender;
                                                    return S_OK;
                                                }
                                            );
                                            store_handler(webview_nav_completed_handler_, h.Get());
                                            HRESULT hrNav = wv2->add_NavigationCompleted(
                                                h.Get(),
                                                &tok
                                            );
                                            std::printf("[Sandbox] add_NavigationCompleted hr=0x%08X\n", (unsigned)hrNav);
                                        }
                                        {
                                            EventRegistrationToken tok{};
                                            auto h = Callback<ICoreWebView2NavigationStartingEventHandler>(
                                                [this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                                    navigation_started_ = true;
                                                    navigation_completed_ = false;
                                                    LPWSTR uri = nullptr;
                                                    if (args) args->get_Uri(&uri);
                                                    if (uri) {
                                                        std::wprintf(L"[Sandbox] NavigationStarting uri=%ls\n", uri);
                                                        CoTaskMemFree(uri);
                                                    } else {
                                                        std::printf("[Sandbox] NavigationStarting\n");
                                                    }
                                                    (void)sender;
                                                    return S_OK;
                                                }
                                            );
                                            store_handler(webview_nav_starting_handler_, h.Get());
                                            HRESULT hrNavS = wv2->add_NavigationStarting(
                                                h.Get(),
                                                &tok
                                            );
                                            std::printf("[Sandbox] add_NavigationStarting hr=0x%08X\n", (unsigned)hrNavS);
                                        }
                                        {
                                            EventRegistrationToken tok{};
                                            auto h = Callback<ICoreWebView2ProcessFailedEventHandler>(
                                                [this](ICoreWebView2* sender, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                                                    COREWEBVIEW2_PROCESS_FAILED_KIND kind = COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
                                                    if (args) args->get_ProcessFailedKind(&kind);
                                                    std::printf("[Sandbox] ProcessFailed kind=%d\n", (int)kind);
                                                    webview_process_failed_ = true;
                                                    (void)sender;
                                                    return S_OK;
                                                }
                                            );
                                            store_handler(webview_process_failed_handler_, h.Get());
                                            HRESULT hrProc = wv2->add_ProcessFailed(
                                                h.Get(),
                                                &tok
                                            );
                                            std::printf("[Sandbox] add_ProcessFailed hr=0x%08X\n", (unsigned)hrProc);
                                        }
                                        if (hwnd_) {
                                            PostMessageW(static_cast<HWND>(hwnd_), WM_NF_SANDBOX_FLUSH_NAV, 0, 0);
                                            SetTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID, NF_SANDBOX_NAV_RETRY_INTERVAL_MS, nullptr);
                                        }
                                    }
                                    std::printf("[Sandbox] WebView2 attached to sandbox window\n");
                                    if (hReady) SetEvent(hReady);
                                    return S_OK;
                                } catch (...) {
                                    std::printf("[Sandbox] Fatal C++ exception in WebView2 controller callback\n");
                                    if (hReady) SetEvent(hReady);
                                    return S_OK;
                                }
                            }
                        ).Get()
                    );
                    std::printf("[Sandbox] CreateCoreWebView2Controller returned hr=0x%08X\n", (unsigned)hrCtl);
                    if (FAILED(hrCtl) && hReady) SetEvent(hReady);
                    return S_OK;
                } catch (...) {
                    std::printf("[Sandbox] Fatal C++ exception in WebView2 environment callback\n");
                    if (hReady) SetEvent(hReady);
                    return S_OK;
                }
            }
        ).Get()
    );
    // Wait briefly for controller creation to complete so first navigation occurs immediately
    if (hReady) {
        DWORD start = GetTickCount();
        MSG msg;
        while (WaitForSingleObject(hReady, 0) == WAIT_TIMEOUT) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (GetTickCount() - start > 5000) { std::printf("[Sandbox] Timeout waiting for WebView2 controller\n"); break; }
            Sleep(10);
        }
        CloseHandle(hReady);
    }
    (void)hrEnv;
#endif
    return true;
#else
    (void)width; (void)height; (void)title; return false;
#endif
}

void WebSandbox::flushPendingNavigation() {
#ifdef _WIN32
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
    if (pending_url_.empty()) {
        if (hwnd_) {
            KillTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID);
        }
        return;
    }
    if (!webview_window_) {
        if (hwnd_) {
            SetTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID, NF_SANDBOX_NAV_RETRY_INTERVAL_MS, nullptr);
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
        std::printf("[Sandbox] Pending navigation gave up after %lu ms attempts=%d\n", (unsigned long)elapsed, pending_nav_attempts_);
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

    auto* wv = static_cast<ICoreWebView2*>(webview_window_);
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
        SetTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID, NF_SANDBOX_NAV_RETRY_INTERVAL_MS, nullptr);
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

bool WebSandbox::navigate(const std::string& url){
#ifdef _WIN32
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
    // Sandbox is observation-only; no sensory data is routed to learning at Stage C v1.
    navigation_requested_ = true;
    pending_url_ = url;
    navigation_completed_ = false;
    webview_process_failed_ = false;
    pending_nav_start_tick_ = 0;
    pending_nav_attempts_ = 0;
    pending_nav_last_attempt_tick_ = 0;
    if (hwnd_) {
        PostMessageW(static_cast<HWND>(hwnd_), WM_NF_SANDBOX_FLUSH_NAV, 0, 0);
        SetTimer(static_cast<HWND>(hwnd_), NF_SANDBOX_NAV_TIMER_ID, NF_SANDBOX_NAV_RETRY_INTERVAL_MS, nullptr);
    }
#endif
    return true;
#else
    (void)url; return false;
#endif
}

bool WebSandbox::scroll(int delta){
#ifdef _WIN32
    // Send a simple mouse wheel event to the sandbox window client area
    if (!hwnd_) return false;
    HWND hwnd = static_cast<HWND>(hwnd_);
    POINT pt{}; GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    SendMessage(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(pt.x, pt.y));
    return true;
#else
    (void)delta; return false;
#endif
}

bool WebSandbox::click(int cx, int cy){
#ifdef _WIN32
    // Send a left-click at client coordinates (cx, cy)
    if (!hwnd_) return false;
    HWND hwnd = static_cast<HWND>(hwnd_);
    LPARAM pos = MAKELPARAM(cx, cy);
    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, pos);
    SendMessage(hwnd, WM_LBUTTONUP, 0, pos);
    return true;
#else
    (void)cx; (void)cy; return false;
#endif
}

bool WebSandbox::typeText(const std::string& text){
#ifdef _WIN32
    if (!hwnd_) return false;
    HWND hwnd = static_cast<HWND>(hwnd_);
    for(char ch : text){
        SendMessage(hwnd, WM_CHAR, static_cast<WPARAM>(static_cast<unsigned char>(ch)), 0);
    }
    return true;
#else
    (void)text; return false;
#endif
}

bool WebSandbox::focus(){
#ifdef _WIN32
    if (!hwnd_) return false;
    HWND hwnd = static_cast<HWND>(hwnd_);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    return true;
#else
    return false;
#endif
}

bool WebSandbox::sendKey(unsigned int vk){
#ifdef _WIN32
    if (!hwnd_) return false;
    HWND hwnd = static_cast<HWND>(hwnd_);
    SendMessage(hwnd, WM_KEYDOWN, static_cast<WPARAM>(vk), 0);
    SendMessage(hwnd, WM_KEYUP, static_cast<WPARAM>(vk), 0);
    return true;
#else
    (void)vk; return false;
#endif
}

::NeuroForge::Sandbox::SandboxRect WebSandbox::bounds() const{
    ::NeuroForge::Sandbox::SandboxRect r{0,0,rect_.w,rect_.h};
    return r;
}
::NeuroForge::Sandbox::SandboxRect WebSandbox::screenBounds() const{
#ifdef _WIN32
    if (!hwnd_) return rect_;
    HWND hwnd = static_cast<HWND>(hwnd_);
    RECT cr{}; GetClientRect(hwnd, &cr);
    POINT tl{0,0}; ClientToScreen(hwnd, &tl);
    ::NeuroForge::Sandbox::SandboxRect r{}; r.x = static_cast<int>(tl.x); r.y = static_cast<int>(tl.y);
    r.w = static_cast<int>(cr.right - cr.left); r.h = static_cast<int>(cr.bottom - cr.top);
    return r;
#else
    return rect_;
#endif
}
bool WebSandbox::isOpen() const{ return hwnd_ != nullptr; }

#ifdef _WIN32
bool WebSandbox::waitUntilReady(int timeout_ms){
    DWORD start = GetTickCount();
    MSG msg;
    for(;;){
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        flushPendingNavigation();
        bool ready = (env_ready_ && controller_ready_ && bounds_update_count_ > 0);
        if (ready && navigation_requested_) {
            ready = pending_url_.empty() || navigation_started_;
        }
        if (ready) return true;
        DWORD elapsed = GetTickCount() - start;
        if (elapsed >= static_cast<DWORD>(timeout_ms)) break;
        Sleep(10);
    }
    std::printf("[Sandbox] waitUntilReady timeout: env=%d controller=%d bounds_updates=%d nav_started=%d nav_completed=%d proc_failed=%d\n",
                env_ready_ ? 1 : 0,
                controller_ready_ ? 1 : 0,
                bounds_update_count_,
                navigation_started_ ? 1 : 0,
                navigation_completed_ ? 1 : 0,
                webview_process_failed_ ? 1 : 0);
    return false;
}
#else
bool WebSandbox::waitUntilReady(int timeout_ms){ (void)timeout_ms; return false; }
#endif

#ifdef _WIN32
void WebSandbox::updateBoundsFromClient(){
    if (!hwnd_) return;
    HWND hwnd = static_cast<HWND>(hwnd_);
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
    if (webview_controller_) {
        auto* ctrl = static_cast<ICoreWebView2Controller*>(webview_controller_);
        RECT rc{}; GetClientRect(hwnd, &rc);
        __try {
            ctrl->put_Bounds(rc);
            ctrl->NotifyParentWindowPositionChanged();
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            webview_controller_ = nullptr;
            controller_ready_ = false;
        }
    }
#endif
    RECT cr{}; GetClientRect(hwnd, &cr);
    rect_.w = static_cast<int>(cr.right - cr.left);
    rect_.h = static_cast<int>(cr.bottom - cr.top);
    bounds_update_count_ += 1;
}
#else
void WebSandbox::updateBoundsFromClient(){}
#endif

} // namespace Sandbox
} // namespace NeuroForge
