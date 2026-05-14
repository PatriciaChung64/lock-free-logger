#pragma once //only compile once if included in multiple files

#include <array>
#include <optional>
#include <atomic>
#include <new>

template <typename T, std::size_t N>
class SPSCQueue {
private:
	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> head{ 0 };
	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> tail{ 0 };
	std::array<T, N> queue;

public:
	SPSCQueue() : queue{} {}

	bool enqueue(const T& value) {
		auto current_head = head.load(std::memory_order_acquire);
		auto current_tail = tail.load(std::memory_order_relaxed);

		if ((current_tail + 1) % N == current_head) {
			return false; // queue is full
		}

		queue[current_tail % N] = value;
		tail.store((current_tail + 1), std::memory_order_release);
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
	SPSCQueue(const SPSCQueue&) = delete;
	SPSCQueue& operator=(const SPSCQueue&) = delete;
	SPSCQueue(SPSCQueue&&) = delete;
	SPSCQueue& operator=(SPSCQueue&&) = delete;
};