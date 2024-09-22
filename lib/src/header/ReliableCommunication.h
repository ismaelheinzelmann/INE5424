#pragma once
#include "Datagram.h"
#include <bits/std_mutex.h>
#include <map>
#include <netinet/in.h>
#include <string>
#include <thread>
#include <vector>
#include "MessageReceiver.h"
#include "../header/Request.h"
#include "../header/BlockingQueue.h"

#ifndef RELIABLE_H
#define RELIABLE_H


class ReliableCommunication{
public:
	ReliableCommunication(std::string configFilePath, unsigned short nodeID);
	~ReliableCommunication();
	void printNodes(std::mutex* printLock) const;
	bool send(unsigned short id, const std::vector<unsigned char>& data);
	void stop();
	bool ackAttempts(int transientSocketfd, sockaddr_in& destin,
	                 Datagram* datagram) const;
	bool dataAttempts(int transientSocketfd, sockaddr_in& destin,
	                  std::vector<unsigned char>& serializedData) const;
	bool dataAttempt(int transientSocketfd, sockaddr_in& destin,
	                 std::vector<unsigned char>& serializedData) const;
	bool ackAttempt(int transientSocketfd, sockaddr_in& destin,
	                std::vector<unsigned char>& serializedData) const;
	bool sendAttempt(int socketfd, sockaddr_in& destin,
	                 std::vector<unsigned char>& serializedData,
	                 Datagram& datagram, int retryMS) const;
	void listen();
	void processDatagram();
	void processDatagram(Datagram* datagram, sockaddr_in* senderAddr) const;
	std::vector<unsigned char>* receive();
	static std::pair<int, sockaddr_in> createUDPSocketAndGetPort();

private:
	int socketInfo;
	unsigned short id;
	MessageReceiver* handler;
	std::map<unsigned short, sockaddr_in> configMap;
	BlockingQueue<std::vector<unsigned char>*> messageQueue;
	BlockingQueue<Request*> requestQueue;
	bool verifyOrigin(sockaddr_in* senderAddr);
	static unsigned short calculateTotalDatagrams(unsigned int dataLength);
	std::thread processingThread;
};


#endif //RELIABLE_H
