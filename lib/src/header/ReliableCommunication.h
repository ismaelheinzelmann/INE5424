#pragma once
#include "Datagram.h"
#include <bits/std_mutex.h>
#include <map>
#include <netinet/in.h>
#include <string>
#include <vector>
#include "MessageReceiver.h"
#include <semaphore>
#ifndef RELIABLE_H
#define RELIABLE_H


class ReliableCommunication{
public:
	ReliableCommunication(std::string configFilePath, unsigned short nodeID);
	~ReliableCommunication();
	void printNodes(std::mutex* printLock) const;
	bool send(unsigned short id, const std::vector<unsigned char>& data);
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
	std::vector<unsigned char>* receive();
	void receiveAndPrint(std::mutex* lock);
	static std::pair<int, sockaddr_in> createUDPSocketAndGetPort();

private:
	int socketInfo;
	unsigned short id;
	MessageReceiver* handler;
	std::map<unsigned short, sockaddr_in> configMap;
	std::queue<std::vector<unsigned char>*> messages;
	std::mutex messagesLock;
	std::counting_semaphore<> semaphore;


	bool verifyOrigin(sockaddr_in& senderAddr);
	static unsigned short calculateTotalDatagrams(unsigned int dataLength);
};


#endif //RELIABLE_H
