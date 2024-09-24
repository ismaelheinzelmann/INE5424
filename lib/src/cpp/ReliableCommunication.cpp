#include "../header/ReliableCommunication.h"

#include <Logger.h>

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
#include <random>
#include <thread>
#define RETRY_ACK_ATTEMPT 3
#define RETRY_ACK_TIMEOUT_USEC 200

#define RETRY_DATA_ATTEMPT 7
#define RETRY_DATA_TIMEOUT_USEC 100
#define RETRY_DATA_TIMEOUT_USEC_MAX 800
#define TIMEOUT_INCREMENT 200
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
}

void ReliableCommunication::stop()
{
	auto endDatagram = Datagram();
	auto flags = Flags{};
	flags.END = true;
	Protocol::sendDatagram(&endDatagram, &configMap[id], socketInfo, &flags);
	while (process)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	if (processingThread.joinable())
	{
		processingThread.join();
	}

	delete handler;
}

bool returnTrueWithProbability(int n)
{
	if (n < 0 || n > 100)
	{
		throw std::invalid_argument("Probability must be between 0 and 100.");
	}

	std::random_device rd; // Get a random number from hardware
	std::mt19937 gen(rd()); // Seed the generator
	std::uniform_int_distribution<> dis(0, 99); // Distribution in range [0, 99]

	int randomValue = dis(gen); // Generate a random number
	return randomValue < n;
}

bool ReliableCommunication::send(const unsigned short id,
                                 const std::vector<unsigned char> &data)
{
	if (this->configMap.find(id) == this->configMap.end())
		return false;
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
	// Start build of datagrams
	auto datagrams = std::vector<std::vector<unsigned char>>(totalDatagrams + 1);
	auto acknowledgments = std::map<unsigned short, bool>();
	for (int i = 0; i < totalDatagrams; ++i)
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
		Protocol::setChecksum(&serializedDatagram);
		datagrams[i] = serializedDatagram;
		acknowledgments[i] = false;
	}

	unsigned short batchSize = 30;
	unsigned short sent = 0;
	const double batchCount = static_cast<int>(ceil(static_cast<double>(totalDatagrams) / batchSize));
	for (unsigned short batchStart = 0; batchStart < batchCount; batchStart++)
	{
		Datagram response;

		unsigned short batchIndex;
		unsigned short batchSent = 0;
		unsigned short batchAck = 0;
		if (sent == totalDatagrams)
		{
			close(transientSocketFd.first);
			return true;
		}
		for (int attempt = 0; attempt < RETRY_DATA_ATTEMPT; attempt++)
		{
			Logger::log("Attempt: " + std::to_string(attempt), LogLevel::DEBUG);
			if (batchAck == batchSize)
				break;

			for (unsigned short j = 0; j < batchSize; j++)
			{
				batchIndex = batchStart * batchSize + j;
				if (batchIndex >= totalDatagrams)
					break;
				if (acknowledgments[batchIndex])
					continue;
				if (returnTrueWithProbability(10))
				{
					Logger::log("Version " + std::to_string(batchIndex + 1) + " lost.", LogLevel::DEBUG);
					batchSent++;
					continue;
				}
				sendto(this->socketInfo, datagrams[batchIndex].data(),
				       datagrams[batchIndex].size(),
				       0,
				       reinterpret_cast<struct sockaddr *>(&destinAddr), sizeof(destinAddr));

			}
			while (Protocol::readDatagramSocketTimeout(response, transientSocketFd.first, destinAddr,
			                                           RETRY_ACK_TIMEOUT_USEC + RETRY_ACK_TIMEOUT_USEC * attempt))
			{
				if (response.getVersion()-1 < batchStart * batchSize || response.getVersion()-1 > (batchStart *
					batchSize) + batchSize)
				{
					continue;
				}
				if (response.getVersion()-1 <= totalDatagrams && response.isACK() && !acknowledgments[response.
					getVersion()-1])
				{
					std::cout << "Datagram of version " << response.getVersion() << " accepted." << std::endl;
					acknowledgments[response.getVersion()-1] = true;
					batchAck++;
					sent++;
				}
				if (response.isACK() && response.isFIN())
				{
					close(transientSocketFd.first);
					return true;
				}
				if (response.isFIN())
				{
					close(transientSocketFd.first);
					return false;
				}
				if (batchAck == batchSize) break;
			}
		}
		if (batchAck != batchSize)
		{
			break;
		}
	}
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
	Logger::log("Listen thread started.", LogLevel::DEBUG);
	processingThread = std::thread([this]
	{
		processDatagram();
	});
	processingThread.detach();
}

void ReliableCommunication::processDatagram()
{
	while (true)
	{
		auto datagram = Datagram();
		auto senderAddr = sockaddr_in{};
		auto buffer = std::vector<unsigned char>(1040);
		Protocol::readDatagramSocket(&datagram, socketInfo, &senderAddr, &buffer);
		buffer.resize(16 + datagram.getDataLength());
		if (!verifyOrigin(&senderAddr))
		{
			continue;
		}
		if (datagram.isEND() && senderAddr.sin_family == this->configMap[id].sin_family &&
			senderAddr.sin_port == this->configMap[id].sin_port &&
			senderAddr.sin_addr.s_addr == this->configMap[id].sin_addr.s_addr)
		{
			process = false;
			return;
		}
		senderAddr.sin_family = AF_INET;
		auto request = Request{&buffer, &senderAddr, &datagram};
		handler->handleMessage(&request, this->socketInfo);
	}
}


std::pair<bool, std::vector<unsigned char>> ReliableCommunication::receive()
{
	return messageQueue.pop();
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

	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd < 0)
	{
		perror("socket");
		throw std::runtime_error("Failed to create socket");
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(0);

	if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		perror("bind");
		close(sockfd);
		throw std::runtime_error("Failed to bind socket");
	}

	if (getsockname(sockfd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) < 0)
	{
		perror("getsockname");
		close(sockfd);
		throw std::runtime_error("Failed to get socket name");
	}

	return {sockfd, addr};
}

