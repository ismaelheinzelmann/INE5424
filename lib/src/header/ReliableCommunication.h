#pragma once
#include "Datagram.h"
#include <bits/std_mutex.h>
#include <map>
#include <netinet/in.h>
#include <string>
#include <thread>
#include <vector>
#include "MessageSender.h"
#include "MessageReceiver.h"
#include "Request.h"
#include "BlockingQueue.h"

#ifndef RELIABLE_H
#define RELIABLE_H


class ReliableCommunication{
public:
	ReliableCommunication(std::string configFilePath, unsigned short nodeID);
	~ReliableCommunication();
	void printNodes(std::mutex* printLock) const;
	bool send(unsigned short id, std::vector<unsigned char>& data);
	void stop();
	void listen();
	// If false, RC stoppend listen
	std::pair<bool,std::vector<unsigned char>> receive();

private:
	int socketInfo;
	unsigned short id;
	bool process = true;
	MessageReceiver* handler;
	MessageSender* sender;
	std::map<unsigned short, sockaddr_in> configMap;
	BlockingQueue<std::pair<bool,std::vector<unsigned char>>> messageQueue;
	BlockingQueue<Request*> requestQueue;

	std::thread processingThread;

	bool verifyOrigin(sockaddr_in* senderAddr);
	void processDatagram();
	static std::pair<int, sockaddr_in> createUDPSocketAndGetPort();
	static unsigned short calculateTotalDatagrams(unsigned int dataLength);
};


#endif //RELIABLE_H
