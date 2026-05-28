#pragma once

#include <new>
#include <atomic>
#include <array>
#include <thread>
#include <fstream>
#include <format>
#include <string>

#include "LogEntryQueue.h"
#include "LogEntry.h"
#include "State.h"

template <std::size_t BATCH_SIZE = 10>
struct LoggerThread
{
    // consumer hot zone
    alignas(std::hardware_destructive_interference_size) LogEntryQueue<10> queue;
    std::atomic<size_t> totalDequeued{0};
    std::atomic<size_t> fullQueueEncounters{0};

    // file write hot zone
    alignas(std::hardware_destructive_interference_size)
    std::array<std::string, BATCH_SIZE> batch;
    std::ofstream file;

    // producer hot zone
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> totalEnqueued{0};

    // control variable
    alignas(std::hardware_destructive_interference_size) std::atomic<State> state{State::Idle};

    // used once per thread
    static inline constexpr char LOG_FILE[] = "log.csv";

    std::thread thread;

    LoggerThread() : batch{}, file(LOG_FILE, std::ios::app), thread([this]() {
        State current_state;
        size_t current_dequeued;
        size_t current_enqueued;
        do {
            current_state = state.load(std::memory_order_acquire);
            if (current_state == State::Idle) {
                std::this_thread::yield();
            }

            else if (current_state == State::Running) {
                auto value = queue.dequeue();
                if (value) {
                    std::string row = std::format("{},{},{}\n", value->timestamp, value->threadID, value->message);
                    current_dequeued = totalDequeued.load(std::memory_order_relaxed);
                    batch[current_dequeued % BATCH_SIZE] = row;
                    current_dequeued++;
                    totalDequeued.fetch_add(1, std::memory_order_relaxed);

                    if (current_dequeued % BATCH_SIZE == 0) {
                        write_batch();
                    }
                }
            }
        } while (current_state == State::Idle || current_state == State::Running);

        //current_state is Stopped, drain the queue and write all remaining to disk
        current_dequeued = totalDequeued.load(std::memory_order_relaxed);
        current_enqueued = totalEnqueued.load(std::memory_order_relaxed);

        while (current_dequeued != current_enqueued) {
            auto value = queue.dequeue();
            if (value) {
                std::string row = std::format("{},{},{}\n", value->timestamp, value->threadID, value->message);
                batch[current_dequeued % BATCH_SIZE] = row;
                totalDequeued.fetch_add(1, std::memory_order_relaxed);
            }
            current_dequeued = totalDequeued.load(std::memory_order_relaxed);
            current_enqueued = totalEnqueued.load(std::memory_order_relaxed);

            if (current_dequeued % BATCH_SIZE == 0) {
                write_batch();
            }
        }

        write_batch(current_dequeued % BATCH_SIZE); //last drain for leftovers
    }) {}

private:
    void write_batch(size_t count = BATCH_SIZE) {
        for (int i = 0; i < count; i++) {
            file << batch[i];
        }
        file.flush();
    }
};