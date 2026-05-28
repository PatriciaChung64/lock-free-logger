#pragma once

#include <new>

enum class LogState {
    Read,
    Writing,
    Written,
    Reading
};

struct alignas(std::hardware_destructive_interference_size) LogEntry {
    static inline constexpr std::size_t MESSAGE_SIZE = 
        std::hardware_destructive_interference_size 
        - sizeof(uint64_t) 
        - sizeof(int) 
        - sizeof(LogState);

    uint64_t timestamp;
    int threadID;
    LogState state;
    char message[MESSAGE_SIZE];
};