#pragma once

#include <new>
#include <atomic>
#include <array>
#include <thread>

#include "LogEntryQueue.h"
#include "LogEntry.h"
#include "State.h"

inline int IDCounter = 0;

int getID() {
    int currentID = IDCounter;
    IDCounter++;
    return currentID;
}

struct WorkerThread
{
    // producer hot zone
    alignas(std::hardware_destructive_interference_size) LogEntryQueue<10> queue;
    std::atomic<size_t> totalEnqueued{0};
    std::atomic<size_t> droppedByProducer{0};
    const int threadID = getID();


    // consumer hot zone
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> totalDequeued{0};

    //contorl variable touched by both
    alignas(std::hardware_destructive_interference_size) std::atomic<State> state{State::Idle};

    // only touch when starting/stopping thread
    std::thread thread;

    WorkerThread(std::optional<int> target) : thread([this, target]() {
        int i = 0;
        State current_state;
        do {
            current_state = state.load(std::memory_order_acquire);
            if (current_state == State::Idle) {
                std::this_thread::yield();
            }
            else if (current_state == State::Running) {
                if (!target || totalEnqueued.load(std::memory_order_relaxed) < *target) {
                    char buf[LogEntry::MESSAGE_SIZE];
                    snprintf(buf, LogEntry::MESSAGE_SIZE, "Thread %d, Item %d", threadID, i);
                    if (queue.enqueue(threadID, buf)) {
                        totalEnqueued.fetch_add(1, std::memory_order_relaxed);
                        i++;
                    }
                    else {
                        droppedByProducer.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        } while (current_state == State::Idle || current_state == State::Running);
    }) {}
};