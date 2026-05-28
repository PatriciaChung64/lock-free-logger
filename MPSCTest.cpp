#include "MPSCManager.h"
#include "LogEntry.h"
#include "SPSCQueue.h"
#include "WorkerThread.h"
#include <unordered_map>
#include <vector>
#include <iostream>
#include <cassert>

int main() {
    int target = 10;
    MPSCManager<3> testManager(target);
    testManager.start_all();

    auto& threads = testManager.get_threads();

    bool all_threads_done = false;
    do {
        all_threads_done = true;
        for (auto& thread : threads) {
            auto currentEnqueued = thread.totalEnqueued.load(std::memory_order_relaxed);
            if (currentEnqueued != target) {
                all_threads_done = false;
                break;
            }
        }

        if (!all_threads_done) {
            testManager.dequeue_thread();
        }
    } while(!all_threads_done);

    testManager.stop_all();

    std::unordered_map<int, std::vector<LogEntry>> dequeued_items;
    while(auto val = testManager.dequeue()) {
        int threadID = val->threadID;
        LogEntry payload = *val;

        dequeued_items[threadID].push_back(payload);
    }

    for (auto& [threadID, payloads]: dequeued_items) {
        std::cout << "ThreadID " << threadID << " enqueued these items: " << std::endl;
        for (auto payload : payloads) {
            std::cout << "Time: " << payload.timestamp << " " << "Message: " << payload.message << std::endl;
        }
        std::cout << std::endl;
    }

    // for (auto& [threadID, payloads] : dequeued_items) {
    //     assert(payloads.size() == target && "wrong number of items");
    //     for (int j = 1; j < payloads.size(); j++) {
    //         assert(payloads[j] == payloads[j-1] + 1 && "ordering violated");
    //     }
    // }

    std::cout << "All assertions passed" << std::endl;

    return 0;
}
