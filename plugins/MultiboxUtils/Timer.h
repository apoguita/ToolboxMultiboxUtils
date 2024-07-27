#pragma once

#include "Headers.h"

class Timer {
public:
    Timer() : start_time(std::clock()), running(false) {}

    void start() {
        start_time = std::clock();
        running = true;
    }

    void stop() {
        running = false;
    }

    bool isStopped() const {
        return !running;
    }

    bool isRunning() const {
        return running;
    }

    bool HasValidData() const {
        return (start_time > 0) ? true : false;
    }

    void reset() {
        start_time = std::clock();
        running = true;
    }

    double getElapsedTime() const {
        if (!running) {
            return 0;
        }
        return ((std::clock() - start_time) / static_cast<double>(CLOCKS_PER_SEC)) * 1000;
    }

    bool hasElapsed(double milliseconds) const {
        if (!running) {
            return false;
        }
        double elapsed_time = getElapsedTime();
        return elapsed_time >= milliseconds;
    }

private:
    std::clock_t start_time;
    bool running;
};
