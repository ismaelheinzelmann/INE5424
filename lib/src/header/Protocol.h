#pragma once
#include <Flags.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/types.h>
#include <thread>
#include <vector>

#include "Datagram.h"

#include <Request.h>
#ifndef PROTOCOL_H
#define PROTOCOL_H

class Protocol {
public:
	static std::vector<unsigned char> serialize(Datagram *datagram);
	static void setChecksum(std::vector<unsigned char> *data);
	static Datagram deserialize(std::vector<unsigned char> &serializedDatagram);
	static unsigned int computeChecksum(std::vector<unsigned char> *serializedDatagram);
	static bool verifyChecksum(Datagram *datagram, std::vector<unsigned char> *serializedDatagram);
	static void signalHandler(int);
	static sockaddr_in broadcastAddress();
	static bool readDatagramSocket(Datagram *datagramBuff, int socketfd, sockaddr_in *senderAddr,
								   std::vector<unsigned char> *buff);
	static bool sendACK(Datagram *datagram, sockaddr_in *to, int socketfd);
	static bool sendNACK(Datagram *datagram, sockaddr_in *to, int socketfd);
	static bool sendSYN(Datagram *datagram, sockaddr_in *to, int socketfd);
	static bool sendDatagram(Datagram *datagram, sockaddr_in *to, int socketfd, Flags *flags);
	static void setFlags(Datagram *datagram, Flags *flags);
	static void setBroadcast(Request *request);
	static unsigned int sumChecksum32(const std::vector<unsigned char> *data);

private:
	// static thread_local std::atomic<bool> waitingTimeout;
	static void bufferToDatagram(Datagram &datagramBuff, const std::vector<unsigned char> &bytesBuffer);
};


#endif // PROTOCOL_H
