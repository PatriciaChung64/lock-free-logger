#pragma once

#include "LoggerThread.h"

template <size_t N, size_t M = 100>
class LoggerManager {
private:
    std::array<LoggerThread, N> threads;
    std::array<LogEntry, M> manager_queue;
    size_t head;
    size_t tail;
    size_t droppedByManager;

public:
    LoggerManager() : threads{}, manager_queue{}, head(0), tail(0), droppedByManager(0) {}

    const std::array<LoggerThread, N>& get_threads() const {
        return threads;
    }

    bool enqueue() {
        
    }
};