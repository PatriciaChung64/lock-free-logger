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

    bool enqueue(uint64_t timestamp, int threadID, const char *message) {
        if ((tail+1) % M == head % M) {
            droppedByManager++;
            return false;
        }

        size_t current_idx = tail % M;
        manager_queue[current_idx].state = LogState::Writing;

        manager_queue[current_idx].timestamp = timestamp;
        manager_queue[current_idx].threadID = threadID;
        snprintf(manager_queue[current_idx].message, LogEntry::MESSAGE_SIZE, "%s", message);

        manager_queue[current_idx].state = LogState::Written;
        tail++;
        return true;
    }

    void distribute_work() {
        
    }
};