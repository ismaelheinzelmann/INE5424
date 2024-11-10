#include "../header/ReliableCommunication.h"

#include "BroadcastType.h"
#include <Logger.h>

#include <cmath>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "BlockingQueue.h"
#include "ConfigParser.h"
#include "MessageSender.h"
#include "Protocol.h"

#include <arpa/inet.h>
#include <cstring>
#include <random>
#include <thread>
#define PORT 8888
// #define BROADCAST_ADDRESS "255.255.255.255"

ReliableCommunication::ReliableCommunication(std::string configFilePath, unsigned short nodeID) {
	this->configMap = ConfigParser::parseNodes(configFilePath);
	this->broadcastType = ConfigParser::parseBroadcast(configFilePath);
	this->faults = ConfigParser::parseFaults(configFilePath);
	this->id = nodeID;
	if (this->configMap.find(id) == this->configMap.end()) {
		throw std::runtime_error("Invalid ID.");
	}
	socketInfo = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketInfo < 0) {
		throw std::runtime_error("Socket could not be created.");
	}
	const sockaddr_in &addr = this->configMap[id];
	if (bind(socketInfo, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0) {
		close(socketInfo);
		throw std::runtime_error("Could not bind socket.");
	}

	// Broadcast
	broadcastInfo = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketInfo < 0) {
		throw std::runtime_error("Socket could not be created.");
	}
	sockaddr_in broadcastAddr{};
	memset(&broadcastAddr, 0, sizeof(broadcastAddr));
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_port = htons(PORT);
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

	// Allow multiple sockets to bind to the same port
	int opt = 1;
	if (setsockopt(broadcastInfo, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		close(broadcastInfo);
		throw std::runtime_error("Could not bind socket.");
	}

	// Allow broadcasting
	if (setsockopt(broadcastInfo, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
		close(broadcastInfo);
		throw std::runtime_error("Could not bind socket.");
	}

	if (bind(broadcastInfo, reinterpret_cast<const sockaddr *>(&broadcastAddr), sizeof(broadcastAddr)) < 0) {
		close(broadcastInfo);
		throw std::runtime_error("Could not bind socket.");
	}
	// End Broadcast

	handler = new MessageReceiver(&messageQueue, &datagramController, &configMap, id, broadcastType, broadcastInfo);
	sender = new MessageSender(socketInfo, broadcastInfo, addr, &datagramController, &configMap, broadcastType);

}

ReliableCommunication::~ReliableCommunication() {
	close(socketInfo);
	close(broadcastInfo);
}

void ReliableCommunication::stop() {
	auto endDatagram = Datagram();
	auto flags = Flags{};
	flags.END = true;
	Protocol::sendDatagram(&endDatagram, &configMap[id], socketInfo, &flags);
	while (process)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	if (processingThread.joinable()) {
		processingThread.join();
	}

	delete handler;
	delete sender;
}

bool ReliableCommunication::send(const unsigned short id, std::vector<unsigned char> &data) {
	if (this->configMap.find(id) == this->configMap.end())
		return false;
	sockaddr_in destin = this->configMap[id];
	return sender->sendMessage(destin, data);
}

bool ReliableCommunication::sendBroadcast(std::vector<unsigned char> &data) { return sender->sendBroadcast(data); }

void ReliableCommunication::listen() {
	Logger::log("Listen thread started.", LogLevel::DEBUG);
	processingThread = std::thread([this] { processDatagram(); });
	processingThread.detach();

	processingBroadcastThread = std::thread([this] { processBroadcastDatagram(); });
	processingBroadcastThread.detach();
}

void ReliableCommunication::processDatagram() {
	while (true) {
		auto datagram = Datagram();
		auto senderAddr = sockaddr_in{};
		auto buffer = std::vector<unsigned char>(1048);
		if (!Protocol::readDatagramSocket(&datagram, socketInfo, &senderAddr, &buffer)) {
			continue;
		}
		if (!verifyOrigin(&datagram)) {
			Logger::log("Message of invalid process received.", LogLevel::DEBUG);
			continue;
		}
		if (datagram.isEND() && datagram.getSourcePort() == this->configMap[id].sin_port &&
			datagram.getSourceAddress() == this->configMap[id].sin_addr.s_addr) {
			process = false;
			return;
		}
		Logger::log("Datagram received.", LogLevel::DEBUG);
		senderAddr.sin_family = AF_INET;
		auto request = Request{&buffer, &senderAddr, &datagram};
		handler->handleMessage(&request, this->socketInfo);
	}
}

void ReliableCommunication::processBroadcastDatagram() {
	while (true) {
		auto datagram = Datagram();
		auto senderAddr = sockaddr_in{};
		auto buffer = std::vector<unsigned char>(1048);
		if (!Protocol::readDatagramSocket(&datagram, broadcastInfo, &senderAddr, &buffer)) {
			continue;
		}
		if (!verifyOriginBroadcast(datagram.getSourcePort()))
		{
			Logger::log("Message of invalid process received.", LogLevel::DEBUG);
			continue;
		}
		if (datagram.isEND() && senderAddr.sin_family == this->configMap[id].sin_family &&
			senderAddr.sin_port == this->configMap[id].sin_port &&
			senderAddr.sin_addr.s_addr == this->configMap[id].sin_addr.s_addr) {
			process = false;
			return;
		}
		senderAddr.sin_family = AF_INET;
		auto request = Request{&buffer, &senderAddr, &datagram};
		handler->handleBroadcastMessage(&request, this->broadcastInfo);
	}
}

std::pair<bool, std::vector<unsigned char>> ReliableCommunication::receive() { return messageQueue.pop(); }

void ReliableCommunication::printNodes(std::mutex *printLock) const {
	std::lock_guard lock(*printLock);
	std::cout << "Nodes:" << std::endl;
	for (const auto &[fst, snd] : this->configMap)
		std::cout << fst << std::endl;
}

BroadcastType ReliableCommunication::getBroadcastType() const
{
	return this->broadcastType;
}

std::pair<int, int> ReliableCommunication::getFaults() const
{
	return this->faults;
}


bool ReliableCommunication::verifyOrigin(Datagram *datagram) {
	for (const auto &[_, nodeAddr] : this->configMap) {
		if (datagram->getSourceAddress() == nodeAddr.sin_addr.s_addr && datagram->getSourcePort() == nodeAddr.sin_port) {
			return true;
		}
	}
	return false;
}

bool ReliableCommunication::verifyOriginBroadcast( int requestSourcePort) {
	for (const auto &[_, nodeAddr] : this->configMap) {
		if (requestSourcePort == nodeAddr.sin_port) {
			return true;
		}
	}
	return false;
}
