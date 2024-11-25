#pragma once
#include <map>

#include "Datagram.h"

#include <netinet/in.h>
#include <vector>
#include "DatagramController.h"

#include "BroadcastType.h"
#include "StatusStruct.h"

#ifndef MESSAGESENDER_H
#define MESSAGESENDER_H


class MessageSender {
public:
	explicit MessageSender(int socketFD, int broadcastFD, sockaddr_in configIdAddr, DatagramController *datagramController,
				  std::map<unsigned short, sockaddr_in> *configMap, BroadcastType broadcastType, StatusStruct *statusStruct);

	~MessageSender() = default;
	bool sendMessage(sockaddr_in &destin, std::vector<unsigned char> &message);
	bool broadcast(std::vector<unsigned char> &message, std::pair<int, sockaddr_in> identifier);
	void buildSynchronizeDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
								   std::map<unsigned short, bool> *acknowledgments, unsigned short totalDatagrams,
								   std::vector<unsigned char> &message, std::pair<unsigned, unsigned short> origin,
								   std::pair<unsigned, unsigned short> identifier);
	bool synchronizeBroadcast(std::vector<unsigned char> &message, std::pair<unsigned, unsigned short> identifier,
							  std::pair<unsigned, unsigned short> target, std::pair<unsigned, unsigned short> origin);
	bool sendBroadcast(std::vector<unsigned char> &message);
	static void removeFailed(
		std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
		std::map<std::pair<unsigned int, unsigned short>, bool> *members);

private:
	int socketFD;
	int broadcastFD;
	sockaddr_in configAddr;
	std::map<unsigned short, sockaddr_in> *configMap;
	DatagramController *datagramController;
	BroadcastType broadcastType;
	StatusStruct *statusStruct;
	std::pair<int, sockaddr_in> createUDPSocketAndGetPort();
	bool verifyMessageAckedURB(std::map<std::pair<unsigned int, unsigned short>, bool> *membersAcks);
	bool verifyMessageAckedFaultyURB(std::map<std::pair<unsigned int, unsigned short>, bool> *membersAcks);
	bool verifyMessageAckedBEB(std::map<std::pair<unsigned int, unsigned short>, bool> *membersAcks);
	static bool verifyMessageAcked(std::map<std::pair<unsigned int, unsigned short>, bool> *membersAcks);
	static bool verifyBatchAcked(
		std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
		unsigned short batchSize, unsigned short batchIndex, unsigned short totalDatagrams);
	static bool verifyBatchAckedFaulty(
		std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
		unsigned short batchSize, unsigned short batchIndex, unsigned short totalDatagrams);
	static unsigned short calculateTotalDatagrams(unsigned int dataLength);
	bool synchronizeAckAttempts(sockaddr_in &destin, Datagram *datagram,
								std::pair<unsigned int, unsigned short> target);

	void buildDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
						std::map<unsigned short, bool> *acknowledgments,
						in_port_t transientPort, unsigned short totalDatagrams, std::vector<unsigned char> &message);
	void buildBroadcastDatagrams(
		std::vector<std::vector<unsigned char>> *datagrams,
		std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
		in_port_t transientPort, unsigned short totalDatagrams, std::vector<unsigned char> &message,
		std::map<std::pair<unsigned int, unsigned short>, bool> *members);
	bool ackAttempts(sockaddr_in &destin, Datagram *datagram);
	bool broadcastAckAttempts(sockaddr_in &destin, Datagram *datagram,
							  std::map<std::pair<unsigned int, unsigned short>, bool> *members);
};


#endif // MESSAGESENDER_H
