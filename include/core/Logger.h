#pragma once

#include <string>
#include <mutex>
#include <fstream>

namespace NeuroForge {
namespace Core {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();
    void setFile(const std::string& path);
    void setLevel(LogLevel level);
    void log(LogLevel level, const std::string& component, const std::string& message);

private:
    Logger();
    std::mutex mtx_;
    std::ofstream file_;
    LogLevel level_{LogLevel::Info};
};

std::string to_string(LogLevel level);

#define NF_LOG_TRACE(component, message) ::NeuroForge::Core::Logger::instance().log(::NeuroForge::Core::LogLevel::Trace, component, message)
#define NF_LOG_DEBUG(component, message) ::NeuroForge::Core::Logger::instance().log(::NeuroForge::Core::LogLevel::Debug, component, message)
#define NF_LOG_INFO(component, message)  ::NeuroForge::Core::Logger::instance().log(::NeuroForge::Core::LogLevel::Info,  component, message)
#define NF_LOG_WARN(component, message)  ::NeuroForge::Core::Logger::instance().log(::NeuroForge::Core::LogLevel::Warn,  component, message)
#define NF_LOG_ERROR(component, message) ::NeuroForge::Core::Logger::instance().log(::NeuroForge::Core::LogLevel::Error, component, message)

} // namespace Core
} // namespace NeuroForge

