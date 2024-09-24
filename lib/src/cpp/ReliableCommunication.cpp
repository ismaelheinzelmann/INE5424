#include "../header/ReliableCommunication.h"

#include <Logger.h>

#include "../header/ConfigParser.h"
#include "../header/Protocol.h"
#include "../header/BlockingQueue.h"
#include "../header/MessageSender.h"
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
	sender = new MessageSender(socketInfo);
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
	delete sender;
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
                                 std::vector<unsigned char> &data)
{
	if (this->configMap.find(id) == this->configMap.end())
		return false;
	sockaddr_in destin = this->configMap[id];
	return sender->sendMessage(destin, data);
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

