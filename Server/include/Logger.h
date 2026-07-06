
#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <mutex>

enum class LoggerMode : uint8_t {
    info = 0,
    error = 1,
    file = 2
};

class Log {
public:
    static Log &instance() {
        static Log logger;
        return logger;
    }
    void operator()(const std::string &text, const LoggerMode z) {
        std::lock_guard<std::mutex> lock(mtx_);
        switch (z) {
            case LoggerMode::info:
                std::cout << text << std::endl;
                break;
            case LoggerMode::error:
                std::cerr << text << std::endl;
                break;
            case LoggerMode::file:
                break;   // todo...
        }
    }
private:
    std::mutex mtx_;
    // ---
    Log() = default;
    ~Log() = default;
    Log(const Log &rhs) = delete;
    Log &operator=(const Log &rhs) = delete;
    Log(Log &&rhs) = delete;
    Log &operator=(Log &&rhs) = delete;
};
