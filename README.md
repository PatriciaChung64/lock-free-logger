## Building
Requires C++ 17 or later.
### Windows (Developer Powershell)
`cl /std:c++17 /EHsc SPSCTest.cpp /I include`
### Linux/Mac
`g++ -std=c++17 -o spsc_test SPSCTest.cpp -I include`

## SPSC Queue

## MPSC Manager
The manager keeps an array of worker threads, all pushing to their own SPSC queue.
The manager goes through each thread in a round robin style to dequeue from each SPSC queue.
For now, the manager then saves the dequeued items in its own container.

### Design decisions
The manager should store each thread as a struct of this shape:
`struct WorkerThread{
    std::thread worker_thread;
    SPSCQueue<T> queue;
    std::atomic<bool> running; // the variable that controls when the worker thread stops
    std::atomic<size_t> totalEnqueued;
    std::atomic<size_t> totalDequeued;
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
- std::array<T> manager_queue - the manager's queue which stores all the items dequeued from threads.
- size_t head - keeps track of the next item to read on the manager's queue
- size_t tail - keeps track of the next memory block to enqueue a new item on the manager's queue

---

### Sizes of the buffer queues
In production, the size of the manager's queue and the worker thread's queues should be calculated based on expected throughput. As the queues themselves are fixed size, and overwrite is not allowed, this size should be sufficient to act as shock absorbers during times of sudden bursts of demand. 

However, since this is a proof of concept project, the sizes of the manager's and workers' queues have been set to an arbitrary size. Currently, the manager's queue is a fixed size of 100, and each worker's queue is a fixed size of 10.

---

### Optimisation trade-off
In theory, the manager's ring buffer queue logic is exactly the same as the SPSCQueue. However, because the SPSCQueue's atomic operations are computationally expensive by imposing memory barriers to avoid instruction reordering and to avoid race conditions in a lock-free design, they are too heavy for the single-threaded manager's use case. To honor the principles of **mechanical sympathy**, we have implmented a lightweight version of this logic for the manager's internal queue to optimise for low-latency.