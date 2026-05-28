#include <iostream>
#include "LoggerThread.h"

int main() {
    LoggerThread testLog;
    size_t test_enqueued = testLog.totalEnqueued.load(std::memory_order_relaxed);
    std::cout << test_enqueued << std::endl;
}