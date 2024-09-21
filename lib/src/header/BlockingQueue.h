#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <mutex>
#include <queue>
#include <semaphore>
#include <stdexcept>

template<typename T>
class BlockingQueue {
public:
	BlockingQueue();

	void push(T value);
	T pop();

private:
	std::mutex mutex_;
	std::counting_semaphore<> semaphore;
	std::queue<T> queue_;
};

// Implementation of the BlockingQueue methods

template<typename T>
BlockingQueue<T>::BlockingQueue() : semaphore(0) {}

template<typename T>
void BlockingQueue<T>::push(T value) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		queue_.push(std::move(value));
	}
	semaphore.release();
}

template<typename T>
T BlockingQueue<T>::pop() {
	semaphore.acquire();
	std::lock_guard<std::mutex> lock(mutex_);
	if (queue_.empty()) {
		throw std::runtime_error("Queue is empty");
	}
	T value = std::move(queue_.front());
	queue_.pop();
	return value;
}

#endif // BLOCKING_QUEUE_H
