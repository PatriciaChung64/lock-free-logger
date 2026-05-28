#pragma once

#include <array>
#include <optional>
#include <atomic>
#include <new>

template <typename T, std::size_t N, typename Derived>
class SPSCQueueBase {
protected:
	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> head{ 0 };
	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> tail{ 0 };
	std::array<T, N> queue;
	
	SPSCQueueBase() : queue{} {}
	
	template <typename... Args>
	void typed_enqueue(std::size_t current_tail, Args&&... args) {
		queue[current_tail % N] = T(std::forward<Args>(args)...);
		tail.store((current_tail + 1), std::memory_order_release);
	}

public:
	template<typename... Args>
	bool enqueue(Args&&... args) {
		auto current_head = head.load(std::memory_order_acquire);
		auto current_tail = tail.load(std::memory_order_relaxed);

		if ((current_tail + 1) % N == current_head % N) {
			return false; // queue is full
		}

		static_cast<Derived*>(this)->typed_enqueue(current_tail, std::forward<Args>(args)...);
		return true;
	}

	std::optional<T> dequeue() {
		auto current_head = head.load(std::memory_order_relaxed);
		auto current_tail = tail.load(std::memory_order_acquire);

		if (current_head == current_tail) {
			return std::nullopt;
		}
		T value = queue[current_head % N];
		head.store((current_head + 1), std::memory_order_release);
		return value;
	}

	// SPSCQueue is neither copyable nor moveable
	SPSCQueueBase(const SPSCQueueBase&) = delete;
	SPSCQueueBase& operator=(const SPSCQueueBase&) = delete;
	SPSCQueueBase(SPSCQueueBase&&) = delete;
	SPSCQueueBase& operator=(SPSCQueueBase&&) = delete;
};

template<typename T, std::size_t N>
class SPSCQueue : public SPSCQueueBase<T, N, SPSCQueue<T, N>> { };
