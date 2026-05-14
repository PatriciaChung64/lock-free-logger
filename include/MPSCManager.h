#include "SPSCQueue.h"
#include <atomic>
#include <array>
#include <new>

enum class State { Idle, Running, Stopped };

template <typename T>
struct WorkerThread
{
    // producer hot zone
    alignas(std::hardware_destructive_interference_size) SPSCQueue<T, 10> queue;
    std::atomic<size_t> totalEnqueued{0};
    std::atomic<State> state{State::Idle};

    // consumer hot zone
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> totalDequeued{0};

    // only touch when starting/stopping thread
    std::thread thread;

    WorkerThread() : thread([this]() {
        int i = 0;
        State current_state;
        do {
            current_state = state.load(std::memory_order_acquire);
            if (current_state == State::Idle) {
                std::this_thread::yield();
            }
            else if (current_state == State::Running) {
                if (queue.enqueue(i)) {
                    totalEnqueued.fetch_add(1, std::memory_order_relaxed);
                    i++;
                }
            }
        } while (current_state == State::Idle || current_state == State::Running);
    }) {}
};

template <typename T, size_t N, size_t M = 100>
class MPSCManager {
    private:
        std::array<WorkerThread<T>, N> threads;
        std::array<T, 100> manager_queue;
        size_t head;
        size_t tail;

    public:
        MPSCManager() : manager_queue{}, head(0), tail(0), threads{} {}

        bool enqueue(T& val) {
            if ((tail+1) % M == head) {
                return false;
            }
            manager_queue[tail%M] = val;
            tail++;
            return true;
        }

        std::optional<T> dequeue() {
            if (tail == head) {
                return std::nullopt;
            }
            T val = manager_queue[head%M];
            head++;
            return val;
        }

        void dequeue_thread() {
            //TODO: dequeue from each thread 1 item and enqueue
        }

        void drain_queue(SPSCQueue<T, 10>& queue, 
            std::atomic<size_t>& totalEnqueued, 
            std::atomic<size_t>& totalDequeued) 
        {
            //safe to load just once since threads are all joined and no further enqueue could happen
            auto current_enqueued = totalEnqueued.load(std::memory_order_relaxed);
            auto current_dequeued = totalDequeued.load(std::memory_order_relaxed);

            do {
                if (auto val = queue.dequeue()) {
                    if (enqueue(val)) current_dequeued++;
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