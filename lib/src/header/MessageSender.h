#pragma once
#include <map>

#include "Datagram.h"

#include <netinet/in.h>
#include <vector>
#include "DatagramController.h"

#ifndef MESSAGESENDER_H
#define MESSAGESENDER_H


class MessageSender {
public:
	explicit MessageSender(int socket_info, int broadcast_info, sockaddr_in addr,
						   DatagramController *datagram_controller, std::map<unsigned short, sockaddr_in> *configMap);

	~MessageSender() = default;
	bool sendMessage(sockaddr_in &destin, std::vector<unsigned char> &message);
	bool sendBroadcast(std::vector<unsigned char> &message);

private:
	int socketFD;
	int broadcastFD;
	sockaddr_in configAddr;
	std::map<unsigned short, sockaddr_in> *configMap;
	DatagramController *datagramController;
	std::pair<int, sockaddr_in> createUDPSocketAndGetPort();
	unsigned short calculateTotalDatagrams(unsigned int dataLength);
	MessageSender(int socketFD, int broadcastFD, sockaddr_in configIdAddr);
	void buildDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
						std::map<unsigned short, bool> *acknowledgments, std::map<unsigned short, bool> *responses,
						in_port_t transientPort, unsigned short totalDatagrams, std::vector<unsigned char> &message);
	void buildBroadcastDatagrams(
		std::vector<std::vector<unsigned char>> *datagrams,
		std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, bool>> *membersAcks,
		in_port_t transientPort, unsigned short totalDatagrams, std::vector<unsigned char> &message);
	bool ackAttempts(sockaddr_in &destin, Datagram *datagram, bool isBroadcast = false);
};


#endif // MESSAGESENDER_H
