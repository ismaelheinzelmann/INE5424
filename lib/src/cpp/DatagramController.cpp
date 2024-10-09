#include "../header/DatagramController.h"

#include <Logger.h>
#include <atomic>
#include <csetjmp>
#include <csignal>
#include <fcntl.h>
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
	{
		std::shared_lock lock(datagramsMutex);
		if (!datagrams.contains(identifier)) {
			return nullptr;
		}
	}

	if (setjmp(jumpBuffer) != 0) {
		ualarm(0, 0);
		return nullptr;
	}
	std::signal(SIGALRM, signalHandler);
	ualarm(timeoutMS * 1000, 0);
	waitingTimeout.store(true);
	Datagram *datagram = datagrams.at(identifier)->pop();
	waitingTimeout.store(false);
	ualarm(0, 0);
	return datagram;
}

thread_local std::atomic<bool> DatagramController::waitingTimeout = false;


void DatagramController::insertDatagram(const std::pair<unsigned int, unsigned short> &identifier, Datagram *datagram) {
	std::unique_lock lock(datagramsMutex);
	auto *newDatagram = new Datagram(datagram);
	if (!datagrams.contains(identifier)) {
		datagrams[identifier] = new BlockingQueue<Datagram *>();
	}
	datagrams[identifier]->push(newDatagram);
}
