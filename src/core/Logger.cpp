#include "core/Logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace NeuroForge {
namespace Core {

static std::string now_ts() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    auto t = system_clock::to_time_t(tp);
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

std::string to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() {}

void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (file_.is_open()) file_.close();
    file_.open(path, std::ios::out | std::ios::app);
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lk(mtx_);
    level_ = level;
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (static_cast<int>(level) < static_cast<int>(level_)) return;
    auto ts = now_ts();
    auto line = ts + " " + to_string(level) + " [" + component + "] " + message;
    if (level == LogLevel::Error || level == LogLevel::Warn) {
        std::cerr << line << std::endl;
    } else {
        std::cout << line << std::endl;
    }
    if (file_.is_open()) {
        file_ << line << std::endl;
    }
}

} // namespace Core
} // namespace NeuroForge

