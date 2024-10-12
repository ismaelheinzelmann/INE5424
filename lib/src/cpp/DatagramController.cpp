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
	sigset_t newmask, oldmask;
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &newmask, &oldmask);
	if (setjmp(jumpBuffer) != 0) {
		pthread_sigmask(SIG_SETMASK, &oldmask, nullptr);
		ualarm(0, 0);
		return nullptr;
	}
	std::signal(SIGALRM, signalHandler);
	waitingTimeout.store(true);
	pthread_sigmask(SIG_UNBLOCK, &newmask, nullptr);
	ualarm(std::min(timeoutMS * 1000, 100000), 0);
	Datagram *datagram = datagrams.at(identifier)->pop();
	pthread_sigmask(SIG_SETMASK, &oldmask, nullptr);
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
