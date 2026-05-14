#pragma once

#include <new>
#include <atomic>
#include <array>
#include <thread>

#include "SPSCQueue.h"
#include "LogEntry.h"

inline int IDCounter = 0;
enum class State { Idle, Running, Stopped };

int getID() {
    int currentID = IDCounter;
    IDCounter++;
    return currentID;
}

struct WorkerThread
{
    // producer hot zone
    alignas(std::hardware_destructive_interference_size) SPSCQueue<LogEntry, 10> queue;
    std::atomic<size_t> totalEnqueued{0};
    std::atomic<size_t> droppedByProducer{0};
    const int threadID = getID();


    // consumer hot zone
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> totalDequeued{0};

    //contorl variable touched by both
    alignas(std::hardware_destructive_interference_size) std::atomic<State> state{State::Idle};

    // only touch when starting/stopping thread
    std::thread thread;

    WorkerThread() : thread([this]() {
        int i = 0;
        State current_state;
        do {
            current_state = state.load(std::memory_order_acquire);
            if (current_state == State::Idle) {
                std::this_thread::yield();
            }
            else if (current_state == State::Running) {
                if (queue.enqueue(LogEntry{threadID, i})) {
                    totalEnqueued.fetch_add(1, std::memory_order_relaxed);
                    i++;
                }
                else {
                    droppedByProducer.fetch_add(1, std::memory_order_relaxed);
                }
            }
        } while (current_state == State::Idle || current_state == State::Running);
    }) {}
};