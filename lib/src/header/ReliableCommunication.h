#pragma once
#include <bits/std_mutex.h>
#include <map>
#include <netinet/in.h>
#include <string>
#include <thread>
#include <vector>
#include "BlockingQueue.h"
#include "Datagram.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "NodeStatus.h"

#include "BroadcastType.h"
#include "StatusStruct.h"
#ifndef RELIABLE_H
#define RELIABLE_H


class ReliableCommunication{
public:
	ReliableCommunication(std::string configFilePath, unsigned short nodeID);
	~ReliableCommunication();
	void printNodes(std::mutex* printLock) const;
	bool send(unsigned short id, std::vector<unsigned char> &data);
	bool sendBroadcast(std::vector<unsigned char> &data);
	BroadcastType getBroadcastType() const;
	std::pair<int, int> getFaults() const;
	int getKeepAliveTime() const;
	void stop();
	void listen();
	// If false, RC stoppend listen
	std::pair<bool,std::vector<unsigned char>> receive();

private:
	int socketInfo;
	int broadcastInfo;
	int keepAliveTime;
	unsigned short id;
	bool process = true;
	MessageReceiver* handler;
	MessageSender* sender;
	std::map<unsigned short, sockaddr_in> configMap;
	BroadcastType broadcastType;
	std::pair<int, int> faults;
	BlockingQueue<std::pair<bool,std::vector<unsigned char>>> messageQueue;
	DatagramController datagramController;

	std::thread processingThread;
	std::thread processingBroadcastThread;

	StatusStruct statusStruct;
	bool verifyOrigin(Datagram *datagram);
	bool verifyOriginBroadcast(int requestSourcePort);
	void processDatagram();
	void configure();
	static bool generateFault(std::vector<unsigned char> *data);
	void processBroadcastDatagram();
	static std::pair<int, sockaddr_in> createUDPSocketAndGetPort();
	static unsigned short calculateTotalDatagrams(unsigned int dataLength);
};


#endif //RELIABLE_H
