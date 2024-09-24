#pragma once
#include "Datagram.h"
#include <netinet/in.h>
#include <vector>
#ifndef MESSAGESENDER_H
#define MESSAGESENDER_H



class MessageSender {
public:
	explicit MessageSender(int socketFD);
	~MessageSender() = default;
	bool sendMessage(sockaddr_in &destin, std::vector<unsigned char> &message);
private:
	int socketFD;
	static std::pair<int, sockaddr_in> createUDPSocketAndGetPort();
	unsigned short calculateTotalDatagrams(unsigned int dataLength);
	bool ackAttempts(int transientSocketfd, sockaddr_in& destin, Datagram* datagram);
};



#endif //MESSAGESENDER_H
