## Building
Requires C++ 17 or later.

To run the SPSCTest (change the method name from SPSCTest() to main() in SPSCTest.cpp):
### Windows (Developer Powershell)
`cl /std:c++17 /EHsc SPSCTest.cpp /I include`
### Linux/Mac
`g++ -std=c++17 -o spsc_test SPSCTest.cpp -I include`

To run the MPSCTest:
### Windows (Developer Powershell)
`cl /std:c++17 /EHsc MPSCTest.cpp /I include`
### Linux/Mac
`g++ -std=c++17 -o spsc_test MPSCTest.cpp -I include`

## SPSC Queue


## MPSC Manager
The manager keeps an array of worker threads, all pushing to their own SPSC queue.
The manager goes through each thread in a round robin style to dequeue from each SPSC queue.
For now, the manager then saves the dequeued items in its own container.

### Design decisions
The manager should store each thread as a struct of this shape:
`struct WorkerThread{
    std::thread worker_thread;
    SPSCQueue<LogEntry> queue;
    std::atomic<bool> running; // the variable that controls when the worker thread stops
    std::atomic<size_t> totalEnqueued;
    std::atomic<size_t> totalDequeued;
    std::atomic<size_t> droppedByProducer;
}`

In particular, the queue, the atomic variable totalEnqueued, and the boolean running is constantly written to or accessed by the worker thread, so we want to ensure that these stay on the same cache line for **cache locality**.

By contrast, totalDequeued is frequently updated by the consumer (manager), so it should stay in its own cache line to avoid **false sharing** with the worker threads.

---

Each WorkerThread:
1. Attempts to push an item to the queue.
2. If successful, fetch_add 1 relaxed on totalEnqueued
3. Repeat 1-3.

---

Dequeue (manager):
1. Dequeue an item from that thread's SPSCQueue
2. If item received (val != nullopt), fetch_add 1 relaxed on totalDequeued.
3. Store item in internal storage

---

Stopping a thread:
1. Set running to false
2. thread.join() to ensure that enqueuing has fully stopped.
3. while (totalDequeued != totalEnqueued), keep dequeuing till queue is drained.

---

What methods should manager have?
- dequeue_thread: See steps 1-3 on Dequeue (manager)
- stop_thread(WorkerThread): stops a particular thread (run step 1-3 for stopping a thread above)
- stop_all: stops all threads
- dequeue: returns an item from the **manager's** queue
- Some helper methods: 
    - get_thread(index) - return a particular thread
    - get_all_threads - return the threads array

---

Therefore, the class variables for the manager should be:
- std::array<WorkerThread, N> threads - array that stores all the threads, number of threads spawned should be given.
- std::array<LogEntry> manager_queue - the manager's queue which stores all the items dequeued from threads.
- size_t head - keeps track of the next item to read on the manager's queue
- size_t tail - keeps track of the next memory block to enqueue a new item on the manager's queue
- size_t droppedByManager - counter that keeps track of dropped items due to full manager queue.

---

### Sizes of the buffer queues
In production, the size of the manager's queue and the worker thread's queues should be calculated based on expected throughput. As the queues themselves are fixed size, and overwrite is not allowed, this size should be sufficient to act as shock absorbers during times of sudden bursts of demand. 

However, since this is a proof of concept project, the sizes of the manager's and workers' queues have been set to an arbitrary size. Currently, the manager's queue is a fixed size of 100, and each worker's queue is a fixed size of 10.

---

### Optimisation trade-off
In theory, the manager's ring buffer queue logic is exactly the same as the SPSCQueue. However, because the SPSCQueue's atomic operations are computationally expensive by imposing memory barriers to avoid instruction reordering and to avoid race conditions in a lock-free design, they are too heavy for the single-threaded manager's use case. To honor the principles of **mechanical sympathy**, we have implmented a lightweight version of this logic for the manager's internal queue to optimise for low-latency.

---

### Dropping items on full
Currently, both the SPSCQueue and MPSCManager's internal aggregated queue **drops enqueued items when the queues are full.** This is a deliberate design decision to ensure **no blocking** happens as it is counterintuitive for threads to block for logging instead of returning to their critical path work. 

If we want items to not be dropped, we have several possible solutions, and both also contradict the design philosophy of the design:

- **Allow overwriting the oldest unread item in the queue to enqueue newest item**: This actually does not solve the problem of dropped updates at all, as you are now dropping old updates instead. If this is a system where newest data should encapsulate old information as well (e.g. stock market trends, machine health monitoring images), then overwriting would make sense. In a logging system, however, every entry has **diagnostic value** and the **integrity of the complete sequence** matters. Overwriting old entries silently removes evidence that may be needed to reconstruct the sequence of events leading to a fault. A dropped new entry is at least a known unknown — you can see the gap in timestamps. An overwritten old entry is an unknown unknown — the log appears complete but is missing data with no indication of the loss.

- **Resizing the queues**: The rationale for the fixed-size ring-buffer design of the current project is to avoid the **memory management jitter** from **heap allocation**. Therefore, allowing for resizing would reintroduce heap allocation, and potentially cause **pointer invalidation** if the queue are storing pointers to objects, and gets moved to a new section of the heap after resizing.

- **Having a temporary back-up store**: Basically just trying to band-aid the problem by adding more **memory management jitter**. Not sustainable and introduces extreme management overhead of having to check two queues instead of one per thread.

- **Increasing the initial size of the queues**: One of the most feasible solutions. Though the calculation on how much to increase requires knowing how many items are actually dropped, in order to know the current limits of the system accurately. This bridges to the next section on monitoring dropped items.

---

### Monitoring dropped items

While it is concluded that dropping items when queues are full is a deliberate design decision based on the principles of the current project, monitoring the number of dropped items could provide important insight for adjusting the initial size of the queues to eliminate chances of dropped items based on the system's maximum load.

The solution is an internal counter at the manager level for its aggregated queue (`droppedManager`), and an individual items dropped counter for each thread monitored by the WorkerThread, such that along with `totalEnqueued` and `totalDequeued`, `droppedByProducer` exposes **queue performance metrics** and is captured by a dedicated worker thread wrapper that resembles an **observer pattern**. 

This design decision of a queue perforamnce metrics wrapper keeps the logic of the SPSCQueue encapsulated, and allows for future **scalability** for more relevant performance metrics monitoring while keeping logic of the SPSCQueue lean.

---

### Testing the MPSC Manager

In order the test the MPSCManager there is a major constraint we must first address in terms of how the thread worker's work cycle is initialised:
```C++
    [this]() {
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
    }
```
This current work cycle while in the running state simply continues enqueuing items forever (if queue is full, the enqueue is dropped, but the loop tries to enqueue the log with the same value until it succeeds).

What this means is that when testing, **we cannot control the amount of data generated by the queue.** This is a massive constraint as the idea of blackbox testing is to insert bounded input and match the output of the system to expectations. As such, we need to refactor this code to **introduce a target number of items to enqueue** so that we can reasonably verify the output. (e.g. if you just let the thread run for 100ms currently, it enqueues ~400 items on my local machine, making debugging very unstable and complex for little reason)

The intuition is therefore to introduce a debug toggle to the lambda, such that if debugg mode is on, the thread will only enqueue the specified number of items, and then remain idle until terminated. This can be solved by **passing a `std::optional<int>` to the lambda**, where **std::nullopt means just enqueue as many items as possible (the current logic)**, and **an int indicates how many items to enqueue** before the work is finished.

The implementation is therefore to pass in the `std::optional<int> target` in the constructor of WorkerThread, so that the lambda can then capture it and use it to determine the number of times to enqueue. 

---

### Parameter pack expansion with the comma operator trick

Reference: https://en.cppreference.com/w/cpp/utility/integer_sequence

Since the WorkerThread now does not have a default constructor, we must provide a value to target like WorkerThread(val) when initialising the WorkerThread array in the MPSCManager. However, since the number of threads is configurable, we need a way to staticly generate at compile time WorkerThread(val) statements based on N, the solution is using the `std::index_sequence<N>` to generate it like so:
```C++
template <std::size_t... Is>
static auto make_workers(std::optional<int> val, std::index_sequence<Is...>) {
    return { ((void)Is, WorkerThread(val))... };
}
```

The template here gives a type name `Is...` for the `std::index_sequence<N>` we passed as function parameter, such that now we can unpack Is... where each Is in Is... creates the instruction `((void)Is, WorkerThread(val))`. In C++, the comma operator evaluates the left operand and discards it, leaving just the right operand `WorkerThread(val)`. Note that the `(void)` cast suppresses potential compiler warnings about unused values when discarding Is. Thus we have { WorkerThread(val), WorkerThread(val) ... } for N times.

---

### MPSCManager correctness test
1. Construct MPSCManager with 3 threads, target of 10 items each
2. start_all()
3. Poll dequeue_thread() in a loop until all threads have hit their target
4. stop_all()
5. Assert correctness:
   - totalEnqueued == target for each thread
   - totalEnqueued == totalDequeued for each thread
   - Each thread's logs arrived in order (payload increments correctly)
   - threadID in each log matches the expected thread
   - droppedByManager == 0 (queue sized generously enough)


