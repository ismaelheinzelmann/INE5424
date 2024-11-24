#pragma once
#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H
#include <condition_variable>
#include "StatusStruct.h"
#include "BlockingQueue.h"

#include <map>
#include <shared_mutex>
#include <thread>
#include <vector>
#include "Datagram.h"
#include "Message.h"
#include "NodeStatus.h"

#include "Request.h"

#include "DatagramController.h"

#include "BroadcastType.h"


// Thread that verifies every N seconds and remove requisitions that timedout;
class MessageReceiver {
public:
	MessageReceiver(BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue,
					DatagramController *datagramController, std::map<unsigned short, sockaddr_in> *configs,
					unsigned short id, const BroadcastType &broadcastType, int broadcastFD, StatusStruct *statusStruct);
	~MessageReceiver();
	void stop();
	void handleMessage(Request *request, int socketfd);
	void treatHeartbeat(Datagram *datagram);
	void handleBroadcastMessage(Request *request);
	std::pair<unsigned int, unsigned short> verifyConsensus();
	void createMessage(Request *request, bool broadcast);
	void configure();
	unsigned getBroadcastMessagesSize();

private:
	std::map<std::pair<in_addr_t, in_port_t>, Message *> messages;
	std::map<std::pair<in_addr_t, in_port_t>, Message *> broadcastMessages;
	std::vector<Message*> broadcastOrder;
	std::shared_mutex messagesMutex;
	BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue;
	std::thread cleanseThread;
	std::thread heartbeatThread;
	StatusStruct *statusStruct;
	NodeStatus status = NOT_INITIALIZED;
	DatagramController *datagramController;
	unsigned short id;
	int broadcastFD;


	// Atomic
	std::atomic<unsigned int> channelIP = 0;
	std::atomic<unsigned short> channelPort = 0;
	std::map<std::pair<in_addr_t, in_port_t>, std::pair<in_addr_t, in_port_t>> heartbeats;
	std::map<std::pair<in_addr_t, in_port_t>, std::chrono::system_clock::time_point> heartbeatsTimes;
	std::mutex heartbeatsLock;

	// Cleanse
	std::condition_variable cv;
	std::mutex mtx;
	std::atomic<bool> running{true};
	std::map<unsigned short, sockaddr_in> *configs;
	BroadcastType broadcastType;

	void handleFirstMessage(Request *request, int socketfd, bool broadcast = false);
	std::pair<unsigned int, unsigned short> getSmallestProcess();
	void deliverBroadcast(Message *message, int broadcastfd);
	void handleBroadcastDataMessage(Request *request);
	static bool verifyMessage(Request *request);
	void handleDataMessage(Request *request, int socketfd);
	bool sendDatagramSYNACK(Request *request, int socketfd);
	bool sendDatagramJOINACK(Request *request, int socketfd);
	bool sendHEARTBEAT(std::pair<unsigned int, unsigned short>, int socketfd);
	Message *getMessage(Datagram *datagram);
	Message *getBroadcastMessage(Datagram *datagram);
	bool sendDatagramACK(Request *request, int socketfd);
	bool sendDatagramFINACK(Request *request, int socketfd);
	bool sendDatagramFIN(Request *request, int socketfd);
	void cleanse();
	void heartbeat();
};


#endif // MESSAGEHANDLER_H
