#include "spinner.hpp"
#include <iostream>
#include <chrono>

Spinner::Spinner(std::string_view label) : label_(label), running_(true) {
    thread_ = std::thread([this]() {
        const int width = 10;
        int position = 0;
        int direction = 1;  // 1 for right, -1 for left
        while (running_) {
            std::string bar = "[";
            for (int i = 0; i < width; ++i) {
                if (i == position) {
                    bar += "█";
                } else {
                    bar += " ";
                }
            }
            bar += "]";
            std::cout << "\r" << label_ << " " << bar << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            position += direction;
            if (position == width - 1 || position == 0) {
                direction = -direction;
            }
        }
        // Clear the line
        std::cout << "\r" << std::string(label_.size() + width + 4, ' ') << "\r" << std::flush;
    });
}

Spinner::~Spinner() {
    stop();
}

void Spinner::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}