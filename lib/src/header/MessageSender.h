#pragma once
#include <map>

#include "Datagram.h"

#include "DatagramController.h"
#include <netinet/in.h>
#include <vector>
#ifndef MESSAGESENDER_H
#define MESSAGESENDER_H


class MessageSender {
public:
	explicit MessageSender(int socketFD, int broadcastFD, sockaddr_in configIdAddr,
						   DatagramController *datagramController);

	~MessageSender() = default;
	bool sendMessage(sockaddr_in &destin, std::vector<unsigned char> &message);
	bool sendBroadcast(std::vector<unsigned char> &message);

private:
	int socketFD;
	int broadcastFD;
	sockaddr_in configAddr;
	DatagramController *datagramController;
	std::pair<int, sockaddr_in> createUDPSocketAndGetPort();
	unsigned short calculateTotalDatagrams(unsigned int dataLength);
	MessageSender(int socketFD, int broadcastFD, sockaddr_in configIdAddr);
	void buildDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
						std::map<unsigned short, bool> *acknowledgments, std::map<unsigned short, bool> *responses,
						in_port_t transientPort, unsigned short totalDatagrams, std::vector<unsigned char> &message);
	void buildBroadcastDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
								 std::map<unsigned short, bool> *acknowledgments,
								 std::map<unsigned short, bool> *responses, in_port_t transientPort,
								 unsigned short totalDatagrams, std::vector<unsigned char> &message);
	bool ackAttempts(sockaddr_in &destin, Datagram *datagram, bool isBroadcast = false);
};


#endif // MESSAGESENDER_H
