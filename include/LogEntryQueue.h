#pragma once

#include <chrono>
#include "SPSCQueue.h"
#include "LogEntry.h"

template <std::size_t N>
class LogEntryQueue : public SPSCQueueBase<LogEntry, N, LogEntryQueue<N>> {
private:
    static uint64_t getTimestamp() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
public:
    void typed_enqueue(std::size_t current_tail, int threadID, const char *message) {
        size_t tail_idx = current_tail % N;

        //set state and start writing new values to the slot
        this->queue[tail_idx].state = LogState::Writing;

        this->queue[tail_idx].threadID = threadID;
        this->queue[tail_idx].timestamp = getTimestamp();
        snprintf(this->queue[tail_idx].message, LogEntry::MESSAGE_SIZE, "%s", message);

        this->queue[tail_idx].state = LogState::Written;
        this->tail.store((current_tail + 1), std::memory_order_release);
    }

    LogEntry typed_dequeue(std::size_t current_head) {
        size_t head_idx = current_head % N;

        //set state and start reading slot
        this->queue[head_idx].state = LogState::Reading;

        //get a copy of the slot
        LogEntry value = this->queue[head_idx];

        //update state at the actual queue
        this->queue[head_idx].state = LogState::Read;
        value.state = LogState::Read; //placeholder logic

        return value;
    }
};