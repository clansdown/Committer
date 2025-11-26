#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <string_view>

class Spinner {
public:
    Spinner(std::string_view label);
    ~Spinner();
    void stop();

private:
    std::string label_;
    std::atomic<bool> running_;
    std::thread thread_;
};