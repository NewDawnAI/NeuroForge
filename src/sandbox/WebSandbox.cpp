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
#include <shellscalingapi.h>
#include <objbase.h>
#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif
#pragma comment(lib, "Shcore.lib")
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
#include <WebView2.h>
#include <wrl.h>
using Microsoft::WRL::Callback;
#endif
#endif

namespace NeuroForge {
namespace Sandbox {

static LRESULT CALLBACK NfSandboxWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_PAINT: {
            PAINTSTRUCT ps; BeginPaint(hWnd, &ps); EndPaint(hWnd, &ps);
            auto* inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (inst) inst->updateBoundsFromClient();
            return 0;
        }
        case WM_SIZE: {
            auto* inst = reinterpret_cast<NeuroForge::Sandbox::WebSandbox*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (inst) inst->updateBoundsFromClient();
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

WebSandbox::WebSandbox() {}
WebSandbox::~WebSandbox() {}

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
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HINSTANCE hInst = GetModuleHandle(nullptr);
    // Register a simple window class dedicated to the sandbox (use wide-char APIs)
    WNDCLASSW wc{}; wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW; wc.lpfnWndProc = NfSandboxWndProc; wc.hInstance = hInst; wc.lpszClassName = L"NfSandboxWnd"; wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if(!RegisterClassW(&wc)){
        return false;
    }
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    RECT r{0,0,width,height}; AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, std::wstring(title.begin(), title.end()).c_str(), style,
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

#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
    // Attempt to initialize Edge WebView2 inside the sandbox window if available
    HANDLE hReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ICoreWebView2Environment* env = nullptr;
    HRESULT hrEnv = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hwnd, hReady](HRESULT result, ICoreWebView2Environment* createdEnv) -> HRESULT {
                if (FAILED(result) || !createdEnv) {
                    std::printf("[Sandbox] WebView2 environment creation failed: hr=0x%08X\n", (unsigned)result);
                    if (hReady) SetEvent(hReady);
                    return S_OK;
                }
                webview_env_ = createdEnv;
                env_ready_ = true;
                std::printf("[Sandbox] WebView2 environment created\n");
                HRESULT hrCtl = createdEnv->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, hReady](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) {
                                std::printf("[Sandbox] WebView2 controller creation failed: hr=0x%08X\n", (unsigned)result);
                                if (hReady) SetEvent(hReady);
                                return S_OK;
                            }
                            webview_controller_ = controller;
                            controller_ready_ = true;
                            std::printf("[Sandbox] WebView2 controller initialized\n");
                            // Obtain the WebView2 core interface and set a solid background to avoid transparent paint
                            ICoreWebView2* wv = nullptr;
                            controller->get_CoreWebView2(&wv);
                            webview_window_ = wv;
                            // Some SDKs expose DefaultBackgroundColor on ICoreWebView2Controller2; use it when available
                            ICoreWebView2Controller2* ctrl2 = nullptr;
                            if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl2))) && ctrl2) {
                                COREWEBVIEW2_COLOR bg{}; bg.A = 255; bg.R = 255; bg.G = 255; bg.B = 255;
                                ctrl2->put_DefaultBackgroundColor(bg);
                                ctrl2->Release();
                            }
                            if (wv) {
                                EventRegistrationToken t1{}; wv->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
                                    [this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                        PWSTR uri = nullptr; args->get_Uri(&uri);
                                        if (uri) { std::wprintf(L"[Sandbox] NavigationStarting: %ls\n", uri); CoTaskMemFree(uri); }
                                        navigation_started_ = true;
                                        return S_OK;
                                    }
                                ).Get(), &t1);
                                EventRegistrationToken t2{}; wv->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                        BOOL ok = FALSE; args->get_IsSuccess(&ok);
                                        COREWEBVIEW2_WEB_ERROR_STATUS st = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN; args->get_WebErrorStatus(&st);
                                        std::wprintf(L"[Sandbox] NavigationCompleted: ok=%d status=%d\n", ok, (int)st);
                                        if (!ok) {
                                            // Fallback: render a minimal HTML page if navigation fails
                                            sender->NavigateToString(L"<html><head><meta charset='utf-8'></head><body><div style='font-family:Segoe UI; padding:24px'><h2>Sandbox</h2><p>Navigation failed; showing fallback content.</p></div></body></html>");
                                        }
                                        return S_OK;
                                    }
                                ).Get(), &t2);
                                EventRegistrationToken t3{}; wv->add_ContentLoading(Callback<ICoreWebView2ContentLoadingEventHandler>(
                                    [](ICoreWebView2* sender, ICoreWebView2ContentLoadingEventArgs* args) -> HRESULT {
                                        BOOL isErrorPage = FALSE; args->get_IsErrorPage(&isErrorPage);
                                        std::wprintf(L"[Sandbox] ContentLoading: error=%d\n", isErrorPage);
                                        return S_OK;
                                    }
                                ).Get(), &t3);
                                EventRegistrationToken t4{}; wv->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                                    [](ICoreWebView2* sender, IUnknown* /*args*/) -> HRESULT {
                                        PWSTR title = nullptr; sender->get_DocumentTitle(&title);
                                        if (title) { std::wprintf(L"[Sandbox] TitleChanged: %ls\n", title); CoTaskMemFree(title); }
                                        return S_OK;
                                    }
                                ).Get(), &t4);
                            }
                            if (hwnd_) {
                                RECT rc{}; GetClientRect(static_cast<HWND>(hwnd_), &rc);
                                controller->put_Bounds(rc);
                                controller->put_IsVisible(TRUE);
                            }
                            if (!pending_url_.empty() && wv) {
                                std::wstring wurl(pending_url_.begin(), pending_url_.end());
                                std::wprintf(L"[Sandbox] Navigating to URL: %ls\n", wurl.c_str());
                                wv->Navigate(wurl.c_str());
                                pending_url_.clear();
                            } else if (wv) {
                                wv->Navigate(L"https://example.com");
                            }
                            std::printf("[Sandbox] WebView2 attached to sandbox window\n");
                            if (hReady) SetEvent(hReady);
                            return S_OK;
                        }
                    ).Get()
                );
                std::printf("[Sandbox] CreateCoreWebView2Controller returned hr=0x%08X\n", (unsigned)hrCtl);
                return S_OK;
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

bool WebSandbox::navigate(const std::string& url){
#ifdef _WIN32
#if defined(NF_HAVE_WEBVIEW2) && defined(_MSC_VER)
    if (webview_window_) {
        auto* wv = static_cast<ICoreWebView2*>(webview_window_);
        std::wstring wurl(url.begin(), url.end());
        std::wprintf(L"[Sandbox] Navigating to URL: %ls\n", wurl.c_str());
        wv->Navigate(wurl.c_str());
        return true;
    }
    pending_url_ = url;
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
        bool ready = (env_ready_ && controller_ready_ && (navigation_started_ || pending_url_.empty()) && bounds_update_count_ > 0);
        if (ready) return true;
        DWORD elapsed = GetTickCount() - start;
        if (elapsed >= static_cast<DWORD>(timeout_ms)) break;
        Sleep(10);
    }
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
        ctrl->put_Bounds(rc);
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
