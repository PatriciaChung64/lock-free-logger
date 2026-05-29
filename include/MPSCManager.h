#pragma once

#include <atomic>
#include <array>

#include "SPSCQueue.h"
#include "LogEntry.h"
#include "WorkerThread.h"

template <std::size_t... Is>
static auto make_workers(std::optional<int> val, std::index_sequence<Is...>) {
    return std::array<WorkerThread, sizeof...(Is)>{ ((void)Is, WorkerThread(val))... };
}

template <size_t N, size_t M = 100>
class MPSCManager {
    private:
        std::array<WorkerThread, N> threads;
        std::array<LogEntry, M> manager_queue;
        size_t head;
        size_t tail;
        size_t droppedByManager;

    public:
        MPSCManager(std::optional<int> target) : manager_queue{}, head(0), tail(0), droppedByManager(0), threads(make_workers(target, std::make_index_sequence<N>{})) {}

        const std::array<WorkerThread, N>& get_threads() const {
            return threads;
        }

        bool enqueue(LogEntry& val) {
            if ((tail+1) % M == head % M) {
                return false;
            }
            manager_queue[tail%M] = val;
            tail++;
            return true;
        }

        std::optional<LogEntry> dequeue() {
            if (tail == head) {
                return std::nullopt;
            }
            LogEntry val = manager_queue[head%M];
            head++;
            return val;
        }

        void dequeue_thread() {
            for(auto& thread : threads) {
                if(auto val = thread.queue.dequeue()) {
                    bool enqueued = enqueue(*val);
                    if (!enqueued) {
                        droppedByManager++;
                    }
                    thread.totalDequeued.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        void drain_queue(LogEntryQueue<10>& queue, 
            std::atomic<size_t>& totalEnqueued, 
            std::atomic<size_t>& totalDequeued) 
        {
            //safe to load just once since threads are all joined and no further enqueue could happen
            auto current_enqueued = totalEnqueued.load(std::memory_order_relaxed);
            auto current_dequeued = totalDequeued.load(std::memory_order_relaxed);

            do {
                if (auto val = queue.dequeue()) {
                    bool enqueued = enqueue(*val);
                    if (!enqueued) {
                        droppedByManager++;
                    }
                    current_dequeued++;
                }
            } while(current_dequeued < current_enqueued);
        }

        void start_all() {
            for(auto& thread : threads) {
                thread.state.store(State::Running, std::memory_order_release);
            }
        }

        void stop_all() {
            for(auto& thread : threads) {
                thread.state.store(State::Stopped, std::memory_order_release);
            }

            for(auto& thread : threads) {
                thread.thread.join();
                drain_queue(thread.queue, thread.totalEnqueued, thread.totalDequeued);
            }
        }
};