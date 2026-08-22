#pragma once

#include <atomic>
#include <functional>
#include <objbase.h>
#include <windows.h>

// Forward declarations of WebView2 interfaces if not included yet
struct ICoreWebView2;
struct ICoreWebView2Environment;
struct ICoreWebView2Controller;
struct ICoreWebView2Settings;

namespace NeuroForge {
namespace Sandbox {

// Base IUnknown implementation to reduce repetition
template <typename THandler> class ComBase : public THandler {
public:
  ComBase(const IID &iid) : iid_(iid) {}
  virtual ~ComBase() = default;

  STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override {
    if (!ppvObject)
      return E_POINTER;
    if (IsEqualIID(riid, __uuidof(IUnknown)) || IsEqualIID(riid, iid_)) {
      *ppvObject = static_cast<THandler *>(this);
      AddRef();
      return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++refCount_; }

  STDMETHODIMP_(ULONG) Release() override {
    ULONG count = --refCount_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

protected:
  std::atomic<ULONG> refCount_{1};
  const IID &iid_;
};

// For Event Handlers: Invoke(Sender*, Args*)
template <typename THandler, typename TArgs>
class ComEventHandler : public ComBase<THandler> {
public:
  using HandlerFunc = std::function<HRESULT(ICoreWebView2 *, TArgs *)>;

  ComEventHandler(const IID &iid, HandlerFunc handler)
      : ComBase<THandler>(iid), handler_(handler) {}

  HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender,
                                   TArgs *args) override {
    if (handler_)
      return handler_(sender, args);
    return S_OK;
  }

private:
  HandlerFunc handler_;
};

// For Creation Handlers: Invoke(HRESULT, CreatedObject*)
template <typename THandler, typename TObject>
class ComCreationHandler : public ComBase<THandler> {
public:
  using HandlerFunc = std::function<HRESULT(HRESULT, TObject *)>;

  ComCreationHandler(const IID &iid, HandlerFunc handler)
      : ComBase<THandler>(iid), handler_(handler) {}

  HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                   TObject *createdObject) override {
    if (handler_)
      return handler_(result, createdObject);
    return S_OK;
  }

private:
  HandlerFunc handler_;
};

} // namespace Sandbox
} // namespace NeuroForge
