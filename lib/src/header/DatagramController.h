#ifndef DATAGRAMCONTROLLER_H
#define DATAGRAMCONTROLLER_H
#include "BlockingQueue.h"
#include "Datagram.h"
#include <atomic>
#include <map>
#include <shared_mutex>


class DatagramController {
public:
	DatagramController() = default;
	~DatagramController() = default;
	void createQueue(std::pair<unsigned int, unsigned short> identifier);
	Datagram *getDatagramTimeout(const std::pair<unsigned int, unsigned short> &identifier, int timeoutMS);
	void insertDatagram(const std::pair<unsigned int, unsigned short> &identifier, Datagram *datagram);
	void flush();
	void deleteQueue(std::pair<unsigned int, unsigned short> identifier);
	std::map<std::pair<unsigned int, unsigned short>, BlockingQueue<Datagram *> *> datagrams;

private:
	static thread_local std::atomic<bool> waitingTimeout;
	std::shared_mutex datagramsMutex;

	static void signalHandler(int);
};


#endif // DATAGRAMCONTROLLER_H
