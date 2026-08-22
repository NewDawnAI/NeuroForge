#ifndef NEUROFORGE_AUDIO_OUTPUT_SYSTEM_H
#define NEUROFORGE_AUDIO_OUTPUT_SYSTEM_H

#include <string>
#include <vector>
#include <future>
#include <iostream>

namespace NeuroForge {
namespace Audio {

class AudioOutputSystem {
public:
    AudioOutputSystem() = default;
    ~AudioOutputSystem() = default;

    bool initialize() {
        // Test if PowerShell is available
        int result = std::system("powershell -Command \"Write-Host 'Audio System Initialized'\"");
        return result == 0;
    }

    void speak(const std::string& text) {
        if (text.empty()) return;
        
        // Run in a separate thread to avoid blocking the main loop
        std::thread([text]() {
            std::string command = "powershell -Command \"Add-Type -AssemblyName System.Speech; "
                                  "$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
                                  "$synth.Speak('" + escapeForShell(text) + "')\"";
            // Suppress output
            command += " > NUL 2>&1";
            std::system(command.c_str());
        }).detach();
    }

private:
    static std::string escapeForShell(const std::string& text) {
        std::string escaped;
        for (char c : text) {
            if (c == '\'') escaped += "''"; // Escape single quotes in PowerShell
            else escaped += c;
        }
        return escaped;
    }
};

} // namespace Audio
} // namespace NeuroForge

#endif // NEUROFORGE_AUDIO_OUTPUT_SYSTEM_H
