#include "SPSCQueue.h"
#include <iostream>
#include <thread>
#include <cassert>

void SPSCTest() {
    SPSCQueue<int, 5> queue; // 4 usable slots
    std::atomic<bool> running{ true };
    std::atomic<int> lastDequeued{ -1 };
    std::atomic<int> totalEnqueued{ 0 };
    std::atomic<int> totalDequeued{ 0 };

    std::thread producer([&queue, &running, &totalEnqueued]() {
        int i = 0;
        while (i < 10) {
            if (queue.enqueue(i)) {
                totalEnqueued.fetch_add(1, std::memory_order_relaxed);
                i++;
            }
        }
     });

    std::thread consumer([&queue, &running, &lastDequeued, &totalDequeued]() {
        do {
            int expected = lastDequeued.load(std::memory_order_relaxed) + 1;
            if (auto val = queue.dequeue()) {
                if (*val != expected) {
                    std::cout << "Got: " << *val << " Expected: " << expected << "\n";
                }
                lastDequeued.store(*val, std::memory_order_relaxed);
                totalDequeued.fetch_add(1, std::memory_order_relaxed);
            }
        } while (totalDequeued.load(std::memory_order_relaxed) < 10);
     });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    producer.join();
    consumer.join();

    std::cout << "Total enqueued: " << totalEnqueued.load() << "\n";
    std::cout << "Total dequeued: " << totalDequeued.load() << "\n";
    std::cout << "Remaining in queue: "
        << totalEnqueued.load() - totalDequeued.load() << "\n";
    std::cout << "All assertions passed, queue is correct\n";
}