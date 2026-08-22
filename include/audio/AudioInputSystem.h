#pragma once

#include <atomic>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>


#ifdef _WIN32
#include <windows.h>
#endif

namespace NeuroForge {
namespace Audio {

class AudioInputSystem {
public:
  AudioInputSystem() : running_(false) {}
  ~AudioInputSystem() { stop(); }

  bool initialize() {
#ifndef _WIN32
    std::cerr << "[STT] AudioInputSystem is currently only supported on "
                 "Windows (PowerShell)."
              << std::endl;
    return false;
#else
    // Test if PowerShell is available
    int result = std::system("powershell -Command \"Write-Host 'Audio Input "
                             "System STT Initializing'\" > NUL 2>&1");
    return result == 0;
#endif
  }

  bool start() {
#ifdef _WIN32
    if (running_.load())
      return true;
    running_.store(true);
    buffer_.clear();

    // Spawn a background PowerShell process that uses System.Speech to listen
    // indefinitely We will read its stdout via a pipe.
    std::thread([this]() {
      // Setup pipes for reading from the child process
      HANDLE hReadPipe, hWritePipe;
      SECURITY_ATTRIBUTES sa;
      sa.nLength = sizeof(SECURITY_ATTRIBUTES);
      sa.bInheritHandle = TRUE;
      sa.lpSecurityDescriptor = NULL;

      if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        std::cerr << "[STT] Failed to create pipe." << std::endl;
        running_.store(false);
        return;
      }

      // Ensure the read handle to the pipe for STDOUT is not inherited
      SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

      // Set up members of the PROCESS_INFORMATION structure
      PROCESS_INFORMATION piProcInfo;
      ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

      // Set up members of the STARTUPINFO structure
      STARTUPINFOW siStartInfo;
      ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
      siStartInfo.cb = sizeof(STARTUPINFOW);
      siStartInfo.hStdError = hWritePipe;
      siStartInfo.hStdOutput = hWritePipe;
      siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
      siStartInfo.wShowWindow = SW_HIDE; // Hide the console window

      // The PowerShell command to run the STT
      // We use DictationGrammar to recognize free-form speech.
      std::wstring cmd =
          L"powershell.exe -NoProfile -NonInteractive -Command \""
          L"Add-Type -AssemblyName System.Speech; "
          L"$rec = New-Object "
          L"System.Speech.Recognition.SpeechRecognitionEngine; "
          L"try { $rec.SetInputToDefaultAudioDevice(); } catch { Write-Host "
          L"'NO_MIC'; exit; } "
          L"$rec.LoadGrammar((New-Object "
          L"System.Speech.Recognition.DictationGrammar)); "
          L"Register-ObjectEvent -InputObject $rec -EventName SpeechRecognized "
          L"-Action { Write-Host $_.EventArgs.Result.Text }; "
          L"Register-ObjectEvent -InputObject $rec -EventName "
          L"RecognizeCompleted -Action { Write-Host 'STT_STOP' }; "
          L"$rec.RecognizeAsync('Multiple'); "
          L"while($true) { Start-Sleep -Seconds 1 }\"";

      std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
      cmdBuffer.push_back(L'\0');

      // Create the child process
      BOOL bSuccess =
          CreateProcessW(NULL,
                         cmdBuffer.data(), // command line
                         NULL,             // process security attributes
                         NULL,             // primary thread security attributes
                         TRUE,             // handles are inherited
                         CREATE_NO_WINDOW, // creation flags
                         NULL,             // use parent's environment
                         NULL,             // use parent's current directory
                         &siStartInfo,     // STARTUPINFO pointer
                         &piProcInfo);     // receives PROCESS_INFORMATION

      if (!bSuccess) {
        std::cerr << "[STT] Failed to create PowerShell process." << std::endl;
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        running_.store(false);
        return;
      }

      // Close the write end of the pipe within the parent process
      CloseHandle(hWritePipe);
      hProcess_ = piProcInfo.hProcess;
      CloseHandle(piProcInfo.hThread);

      // Read output from the child process's pipe
      DWORD dwRead;
      CHAR chBuf[4096];
      std::string current_line;

      while (running_.load()) {
        // Check if process is still running and there's data to read
        DWORD dwAvail = 0;
        if (!PeekNamedPipe(hReadPipe, NULL, 0, NULL, &dwAvail, NULL) ||
            dwAvail == 0) {
          DWORD waitResult = WaitForSingleObject(hProcess_, 100);
          if (waitResult == WAIT_OBJECT_0) {
            break; // Process terminated
          }
          continue;
        }

        bSuccess = ReadFile(hReadPipe, chBuf, sizeof(chBuf) - 1, &dwRead, NULL);
        if (!bSuccess || dwRead == 0)
          break;

        chBuf[dwRead] = '\0';
        std::string chunk(chBuf, dwRead);

        // Parse lines
        for (char c : chunk) {
          if (c == '\n') {
            // Clean up CR
            if (!current_line.empty() && current_line.back() == '\r') {
              current_line.pop_back();
            }
            if (current_line == "NO_MIC") {
              std::cerr << "[STT] No microphone found or accessible."
                        << std::endl;
            } else if (current_line == "STT_STOP") {
              // Recognition engine stopped naturally
            } else if (!current_line.empty()) {
              std::lock_guard<std::mutex> lock(mtx_);
              buffer_.push_back(current_line);
            }
            current_line.clear();
          } else {
            current_line += c;
          }
        }
      }

      // Cleanup
      CloseHandle(hReadPipe);
      if (hProcess_) {
        TerminateProcess(hProcess_, 0);
        CloseHandle(hProcess_);
        hProcess_ = NULL;
      }
      running_.store(false);
    }).detach();

    return true;
#else
    return false;
#endif
  }

  void stop() {
    running_.store(false);
    // Process handles cleaned up in the reading thread once running_ falls
    // entirely.
  }

  // Returns a queue of successfully recognized speech transcripts
  // since the last fetch.
  std::vector<std::string> fetch() {
    std::vector<std::string> lines;
    std::lock_guard<std::mutex> lock(mtx_);
    while (!buffer_.empty()) {
      lines.push_back(buffer_.front());
      buffer_.pop_front();
    }
    return lines;
  }

private:
  std::atomic<bool> running_;
  std::mutex mtx_;
  std::deque<std::string> buffer_;
#ifdef _WIN32
  HANDLE hProcess_ = NULL;
#endif
};

} // namespace Audio
} // namespace NeuroForge
