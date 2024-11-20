#pragma once
#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H
#include <condition_variable>

#include "BlockingQueue.h"

#include <map>
#include <shared_mutex>
#include <thread>
#include <vector>
#include "Datagram.h"
#include "Message.h"

#include "Request.h"

#include "DatagramController.h"

#include "BroadcastType.h"

#include "StatusDTO.h"


// Thread that verifies every N seconds and remove requisitions that timedout;
class MessageReceiver {
public:
	MessageReceiver(BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue,
					DatagramController *datagramController, std::map<unsigned short, sockaddr_in> *configs,
					unsigned short id, const BroadcastType &broadcastType, int broadcastFD, StatusDTO statusDTO);
	~MessageReceiver();
	void stop();
	void handleMessage(Request *request, int socketfd);
	void handleBroadcastMessage(Request *request, int socketfd);
	void createMessage(Request *request, bool broadcast);
	void consensusSYNACK(Request *request, int socketfd, unsigned order);

private:
	std::map<std::pair<in_addr_t, in_port_t>, Message *> messages;
	std::shared_mutex messagesMutex;
	BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue;
	std::thread cleanseThread;
	std::thread heartbeatThread;

	DatagramController *datagramController;
	unsigned short id;
	int broadcastFD;
	StatusDTO status;

	std::map<unsigned int, Message*> messageOrder;
	unsigned int lastMessage = 0;

	// Atomic
	std::atomic_bool channelOccupied;
	std::atomic<unsigned int> channelMessageIP = 0;
	std::atomic<unsigned short> channelMessagePort = 0;

	// Cleanse
	std::condition_variable cv;
	std::mutex mtx;
	std::atomic<bool> running{true};
	std::map<unsigned short, sockaddr_in> *configs;
	BroadcastType broadcastType;

	void handleFirstMessage(Request *request, int socketfd, bool broadcast = false);
	unsigned getOrder(Datagram *datagram);
	unsigned short getMemberSize();
	void deliverBroadcast(Message *message, int broadcastfd);
	void handleBroadcastDataMessage(Request *request, int socketfd);
	static bool verifyMessage(Request *request);
	void handleDataMessage(Request *request, int socketfd);
	bool sendDatagramSYNACK(Request *request, int socketfd);
	bool sendHEARTBEAT(int socketfd);
	Message *getMessage(Datagram *datagram);
	bool sendDatagramACK(Request *request, int socketfd);
	bool sendDatagramFINACK(Request *request, int socketfd);
	bool sendDatagramFIN(Request *request, int socketfd);
	void cleanse();
	void heartbeat();
};


#endif // MESSAGEHANDLER_H
