#include "../header/ReliableCommunication.h"
#include "../header/ConfigParser.h"
#include "../header/Protocol.h"
#include "../header/BlockingQueue.h"
#include <TypeUtils.h>
#include <cmath>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <thread>
#define RETRY_ACK_ATTEMPT 4
#define RETRY_ACK_TIMEOUT_USEC 250

#define RETRY_DATA_ATTEMPT 8
#define RETRY_DATA_TIMEOUT_USEC 250
// clang-format off

ReliableCommunication::ReliableCommunication(std::string configFilePath, unsigned short nodeID)
{
	this->configMap = ConfigParser::parseNodes(configFilePath);
	this->id = nodeID;
	if (this->configMap.find(id) == this->configMap.end())
	{
		throw std::runtime_error("Invalid ID.");
	}
	socketInfo = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketInfo < 0)
	{
		throw std::runtime_error("Socket could not be created.");
	}
	const sockaddr_in &addr = this->configMap[id];
	if (bind(socketInfo, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		close(socketInfo);
		throw std::runtime_error("Could not bind socket.");
	}

	handler = new MessageReceiver(&messageQueue, &requestQueue);
}

ReliableCommunication::~ReliableCommunication()
{
	close(socketInfo);
	delete handler;
}

bool ReliableCommunication::send(const unsigned short id,
                                 const std::vector<unsigned char> &data)
{
	if (this->configMap.find(id) == this->configMap.end())
	{
		throw std::runtime_error("Invalid ID.");
	}
	std::pair<int, sockaddr_in> transientSocketFd = createUDPSocketAndGetPort();

	auto datagram = Datagram();
	unsigned short totalDatagrams = calculateTotalDatagrams(data.size());
	datagram.setDatagramTotal(totalDatagrams);
	datagram.setSourcePort(transientSocketFd.second.sin_port);
	sockaddr_in destinAddr = this->configMap[id];
	bool accepted = ackAttempts(transientSocketFd.first, destinAddr, &datagram);
	if (!accepted)
	{
		return false;
	}
	// Verify if its faster to pre compute serialization
	// TODO Pre computar o proximo?
	for (unsigned short i = 0; i < totalDatagrams; i++)
	{
		auto versionDatagram = Datagram();
		versionDatagram.setSourcePort(transientSocketFd.second.sin_port);
		versionDatagram.setVersion(i + 1);
		versionDatagram.setDatagramTotal(totalDatagrams);
		for (unsigned short j = 0; j < 1024; j++)
		{
			const unsigned int index = i * 1024 + j;
			if (index >= data.size())
				break;
			versionDatagram.getData()->push_back(data.at(index));
		}
		versionDatagram.setDataLength(versionDatagram.getData()->size());
		auto serializedDatagram = Protocol::serialize(&versionDatagram);
		unsigned char checksum[4] = {0, 0, 0, 0};
		// TypeUtils::uintToBytes(Protocol::computeChecksum(&serializedDatagram), checksum);
		for (unsigned short j = 0; j < 4; j++)
			serializedDatagram[12 + j] = checksum[j];
		Datagram response;
		for (int j = 0; j < RETRY_DATA_ATTEMPT; ++j)
		{
			const ssize_t bytes = sendto(this->socketInfo, serializedDatagram.data(), serializedDatagram.size(), 0,
			                             reinterpret_cast<struct sockaddr *>(&destinAddr), sizeof(destinAddr));
			if (bytes < 0)
			{
				std::cout << "Failed sending datagram." << std::endl;
				continue;
			}

			sockaddr_in senderAddr{};
			senderAddr.sin_family = AF_INET;
			bool answer = Protocol::readDatagramSocketTimeout(response, transientSocketFd.first, senderAddr,
			                                                  RETRY_DATA_TIMEOUT_USEC + RETRY_DATA_TIMEOUT_USEC * j);
			// TODO Validate if is really the right sender
			// TODO Validate checksum
			if (!answer)
			{
				std::cout << "Receiver didn't respond ACK for version " << i << std::endl;
				continue;
			}
			if (response.isNACK())
			{
				std::cout << "Receiver didn't agree with version " << i << std::endl;
				continue;
			}

			if (response.getVersion() == totalDatagrams && response.isACK() && response.isFIN())
			{
				std::cout << "Receiver ended the connection, message successfully sent." << std::endl;
				close(transientSocketFd.first);
				return true;
			}
			if (response.isACK() && response.getVersion() == i + 1)
			{
				break;
			}
			if (response.isFIN())
				return false;
		}
	}
	std::cout<<"Failed sending message" << std::endl;
	close(transientSocketFd.first);
	return false;
}

bool ReliableCommunication::ackAttempts(int transientSocketfd, sockaddr_in &destin, Datagram *datagram) const
{
	Flags flags;
	flags.SYN = true;
	Datagram response;
	Protocol::setFlags(datagram, &flags);

	for (int i = 0; i < RETRY_ACK_ATTEMPT; ++i)
	{
		bool sent = Protocol::sendDatagram(datagram, &destin, socketInfo, &flags);
		if (!sent)
		{
			std::cout << "Failed sending datagram." << std::endl;
			continue;
		}
		sockaddr_in senderAddr{};
		senderAddr.sin_family = AF_INET;
		sent = Protocol::readDatagramSocketTimeout(response, transientSocketfd, senderAddr,
		                                           RETRY_ACK_TIMEOUT_USEC + RETRY_ACK_TIMEOUT_USEC * i);
		if (!sent)
			continue;
		// if (!Protocol::verifyChecksum(response)) continue;
		if (response.isACK() && response.isSYN() && datagram->getVersion() == response.getVersion())
		{
			// std::cout << "ACK Accepted." << std::endl;
			return true;
		}
	}
	std::cout << "Failed ack." << std::endl;
	return false;
}

bool ReliableCommunication::sendAttempt(int socketfd, sockaddr_in &destin, std::vector<unsigned char> &serializedData,
                                        Datagram &datagram, int retryMS) const
{
	const ssize_t bytes = sendto(this->socketInfo, serializedData.data(), serializedData.size(), 0,
	                             reinterpret_cast<struct sockaddr *>(&destin), sizeof(destin));
	if (bytes < 0)
	{
		return false;
	}
	sockaddr_in senderAddr{};
	senderAddr.sin_family = AF_INET;
	return Protocol::readDatagramSocketTimeout(datagram, socketfd, senderAddr, retryMS);
}


void ReliableCommunication::listen()
{
	std::thread processingThread([this]
	{
		processDatagram();
	});
	processingThread.detach();
}

void ReliableCommunication::processDatagram()
{
	while (true)
	{
		const auto datagram = new Datagram();
		const auto senderAddr = new sockaddr_in{};
		const auto buffer = new std::vector<unsigned char>(1040);
		Protocol::readDatagramSocket(datagram, socketInfo, senderAddr, buffer);

		if (!verifyOrigin(senderAddr))
		{
			continue;
		}
		senderAddr->sin_family = AF_INET;
		auto request = new Request{buffer, senderAddr, datagram};
		handler->handleMessage(request, this->socketInfo);
	}
}

std::vector<unsigned char> *ReliableCommunication::receive()
{
	return messageQueue.pop();;
}

void ReliableCommunication::printNodes(std::mutex *printLock) const
{
	std::lock_guard lock(*printLock);
	std::cout << "Nodes:" << std::endl;
	for (const auto &[fst, snd] : this->configMap)
		std::cout << fst << std::endl;
}

unsigned short ReliableCommunication::calculateTotalDatagrams(unsigned int dataLength)
{
	const double result = static_cast<double>(dataLength) / 1024;
	return static_cast<int>(ceil(result));
}


void ReliableCommunication::receiveAndPrint(std::mutex *lock)
{
	while (true)
	{
		auto message = receive();
		std::string messageString(message->begin(), message->end());
		lock->lock();
		lock->lock();
		std::cout << messageString << std::endl;
		lock->unlock();
	}
}

bool ReliableCommunication::verifyOrigin(sockaddr_in *senderAddr)
{
	for (const auto &[_, nodeAddr] : this->configMap)
	{
		if (senderAddr->sin_addr.s_addr == nodeAddr.sin_addr.s_addr && senderAddr->sin_port == nodeAddr.sin_port)
		{
			return true;
		}
	}
	return false;
}

std::pair<int, sockaddr_in> ReliableCommunication::createUDPSocketAndGetPort()
{
	sockaddr_in addr{};
	socklen_t addr_len = sizeof(addr);

	// Create a UDP socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd < 0)
	{
		perror("socket");
		throw std::runtime_error("Failed to create socket");
	}

	// Prepare sockaddr_in structure for a dummy bind
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(0); // Let the system choose an available port

	// Bind the socket to get an available port (dummy bind)
	if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		perror("bind");
		close(sockfd);
		throw std::runtime_error("Failed to bind socket");
	}

	// Retrieve the assigned port
	if (getsockname(sockfd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) < 0)
	{
		perror("getsockname");
		close(sockfd);
		throw std::runtime_error("Failed to get socket name");
	}

	// Return the file descriptor and the address
	return {sockfd, addr};
}

