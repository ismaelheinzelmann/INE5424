#pragma once
#include <map>

#include "Datagram.h"
#include <netinet/in.h>
#include <vector>
#ifndef MESSAGESENDER_H
#define MESSAGESENDER_H


class MessageSender{
public:
	explicit MessageSender(int socketFD);
	static void buildDatagrams(std::vector<std::vector<unsigned char>>* datagrams,
	                           std::map<unsigned short, bool>* acknowledgments,
	                           std::map<unsigned short, bool>* responses,
	                           in_port_t transientPort, unsigned short totalDatagrams,
	                           std::vector<unsigned char>& message);
	~MessageSender() = default;
	bool sendMessage(sockaddr_in &destin, std::vector<unsigned char> &message);
	bool sendBroadcast(std::vector<unsigned char> &message);
	static sockaddr_in broadcastAddress();

private:
	int socketFD;
	static std::pair<int, sockaddr_in> createUDPSocketAndGetPort();
	unsigned short calculateTotalDatagrams(unsigned int dataLength);
	bool ackAttempts(int transientSocketfd, sockaddr_in& destin, Datagram* datagram, bool isBroadcast = false);
};


#endif //MESSAGESENDER_H
