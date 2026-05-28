#pragma once

#include "SPSCQueue.h"
#include "LogEntry.h"

template <std::size_t N>
class LogEntryQueue : public SPSCQueueBase<LogEntry, N, LogEntryQueue<N>> {
public:
    void typed_enqueue(std::size_t current_tail, int threadID, int payload) {
        this->queue[current_tail % N].threadID = threadID;
        this->queue[current_tail % N].payload = payload;

        this->tail.store((current_tail + 1), std::memory_order_release);
    }
};