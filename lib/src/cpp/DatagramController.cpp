#include "../header/DatagramController.h"

#include <Logger.h>
#include <atomic>
#include <chrono>
#include <csetjmp>
#include <csignal>
#include <fcntl.h>
#include <future>
#include <memory>
#include <shared_mutex>
thread_local jmp_buf jumpBuffer;

void DatagramController::createQueue(std::pair<unsigned int, unsigned short> identifier) {
	std::unique_lock lock(datagramsMutex);
	if (!datagrams.contains(identifier)) {
		datagrams[identifier] = new BlockingQueue<Datagram *>();
	}
}

void DatagramController::signalHandler(int) {
	if (waitingTimeout.load()) {
		waitingTimeout.store(false);
		longjmp(jumpBuffer, 1);
	}
}

Datagram *DatagramController::getDatagramTimeout(const std::pair<unsigned int, unsigned short> &identifier,
												  int timeoutMS) {
	BlockingQueue<Datagram *> *datagramResource;

	{
		std::shared_lock lock(datagramsMutex);
		auto it = datagrams.find(identifier);
		if (it == datagrams.end()) {
			return nullptr;
		}
		datagramResource = it->second;
	}

	try {
		return datagramResource->popWithTimeout(std::chrono::milliseconds(timeoutMS));
	} catch (const std::runtime_error &e) {
		return nullptr;
	}
}

thread_local std::atomic<bool> DatagramController::waitingTimeout = false;

void DatagramController::deleteQueue(std::pair<unsigned int, unsigned short> identifier) {
	std::unique_lock lock(datagramsMutex);
	if (datagrams.contains(identifier)) {
		datagrams.erase(identifier);
	}
}

void DatagramController::insertDatagram(const std::pair<unsigned int, unsigned short> &identifier, Datagram *datagram) {
	std::unique_lock lock(datagramsMutex);
	auto *newDatagram = new Datagram(datagram);
	if (!datagrams.contains(identifier)) {
		datagrams[identifier] = new BlockingQueue<Datagram *>();
	}
	datagrams[identifier]->push(newDatagram);
}
