#include "../header/MessageReceiver.h"

#include <iostream>
#include <map>
#include <shared_mutex>
#include "BlockingQueue.h"
#include "BroadcastType.h"
#include "Datagram.h"
#include "Logger.h"
#include "Protocol.h"
#include "Request.h"

MessageReceiver::MessageReceiver(BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue,
								 DatagramController *datagramController, std::map<unsigned short, sockaddr_in> *configs,
								 unsigned short id, const BroadcastType &broadcastType) {
	this->messages = std::map<std::pair<in_addr_t, in_port_t>, Message *>();
	this->messageQueue = messageQueue;
	this->datagramController = datagramController;
	cleanseThread = std::thread([this] { cleanse(); });
	cleanseThread.detach();
	this->configs = configs;
	this->id = id;
	this->broadcastType = broadcastType;
}

MessageReceiver::~MessageReceiver() {
	{
		std::lock_guard lock(mtx);
		running = false;
	}
	messageQueue->push(std::make_pair(false, std::vector<unsigned char>()));
	running = false;
	cv.notify_all();
	if (cleanseThread.joinable())
		cleanseThread.join();
	std::lock_guard messagesLock(messagesMutex);
	for (auto it = this->messages.begin(); it != this->messages.end();) {
		auto &[pair, message] = *it;
		{
			std::lock_guard messageLock(*message->getMutex());
			delete message;
		}
		it = messages.erase(it);
	}
}


void MessageReceiver::cleanse() {
	while (true) {
		{
			std::unique_lock lock(mtx);
			if (cv.wait_for(lock, std::chrono::seconds(10), [this] { return !running; })) {
				return;
			}
		}
		{
			std::lock_guard messagesLock(messagesMutex);
			auto remove = std::vector<std::pair<unsigned int, unsigned short>>();
			for (auto [identifier, message] : messages) {
				if (std::chrono::system_clock::now() - message->getLastUpdate() > std::chrono::seconds(10)) {
					remove.emplace_back(identifier);
				}
			}
			for (auto identifier : remove) {
				this->messages.erase(identifier);
				datagramController->deleteQueue(identifier);
			}
		}
	}
}


bool MessageReceiver::verifyMessage(Request *request) {
	return Protocol::verifyChecksum(request->datagram, request->data);
}

void MessageReceiver::handleDataMessage(Request *request, int socketfd) {
	std::shared_lock lock(messagesMutex);
	Message *message = getMessage(request->datagram);
	if (message == nullptr) {
		sendDatagramFIN(request, socketfd);
		return;
	}
	std::lock_guard messageLock(*message->getMutex());

	if (message->delivered) {
		sendDatagramFINACK(request, socketfd);
		return;
	}
	if (!message->verifyMessage(*(request->datagram))) {
		return;
	}
	bool sent = message->addData(request->datagram);
	if (sent) {
		sendDatagramFINACK(request, socketfd);
		if (!message->delivered) {
			message->delivered = true;
			messageQueue->push(std::make_pair(true, *message->getData()));
		}
	}
	sendDatagramACK(request, socketfd);
}

void MessageReceiver::handleMessage(Request *request, int socketfd) {
	std::shared_lock lock(messagesMutex);
	if (!verifyMessage(request)) {
		return;
	}

	// Data datagram
	if (request->datagram->getFlags() == 0) {
		handleDataMessage(request, socketfd);
		return;
	}
	if ((request->datagram->isSYN() && request->datagram->isACK()) ||
		(request->datagram->isFIN() && request->datagram->isACK()) || request->datagram->isACK() ||
		request->datagram->isFIN()) {
		datagramController->insertDatagram(
			{request->datagram->getSourceAddress(), request->datagram->getDestinationPort()}, request->datagram);
		return;
	}
	if (request->datagram->isSYN()) {
		handleFirstMessage(request, socketfd);
	}
}

void MessageReceiver::handleBroadcastMessage(Request *request, int socketfd) {
	std::shared_lock lock(messagesMutex);
	if (!verifyMessage(request)) {
		return;
	}
	Protocol::setBroadcast(request);
	Datagram *datagram = request->datagram;
	if (broadcastType == AB) {
		std::pair id = {request->datagram->getSourceAddress(), request->datagram->getDestinationPort()};
		// PROBLEMA DEVE TA AQ EM BAIXO
		if (channelOccupied && channelMessageIP != id.first && channelMessagePort != id.second
			&& (channelMessageIP != 0 && channelMessagePort != 0)) {
			std::pair<unsigned int, unsigned short> identifier = {channelMessageIP, channelMessagePort};
			if (!messages[identifier]->delivered) {
				std::cout<<"MESSAGE REFUSED"<<std::endl;
				return;
			}
			std::pair consent = verifyConsensus();
			if (messages.contains(consent)) {
				if (!messages[consent]->delivered) {
					std::cout<<"MESSAGE REFUSED 00"<<std::endl;
					return;
				}
			}
		}
	}
	// Data datagram
	if (datagram->getFlags() == 0) {
		handleBroadcastDataMessage(request, socketfd);
		return;
	}
	if ((datagram->isSYN() && datagram->isACK()) || (datagram->isFIN() && datagram->isACK()) || datagram->isACK() ||
		datagram->isFIN()) {
		std::shared_lock lock(messagesMutex);
		Message *message = getMessage(datagram);
		if (message == nullptr)
			return;
		if ((datagram->isFIN() || datagram->isSYN()) && datagram->isACK()) {
			std::shared_lock messageLock(*message->getMutex());
			std::pair identifier = {datagram->getSourceAddress(), datagram->getSourcePort()};

			if (!message->acks.contains(identifier) && datagram->isSYN()) {
				message->acks[identifier] = false;
			}
			else if (!message->acks.contains(identifier) && datagram->isFIN()) {
				return;
			}
			else {
				message->acks[identifier] = datagram->isFIN();
				if (!message->delivered && datagram->isFIN()) {
					deliverBroadcast(message);
				}
			}
		}
		datagramController->insertDatagram({datagram->getSourceAddress(), datagram->getDestinationPort()}, datagram);
		return;
	}
	if (datagram->isSYN()) {
		handleFirstMessage(request, socketfd, true);
	}
}

std::pair<unsigned int, unsigned short> MessageReceiver::verifyConsensus() {
	std::map<std::pair<in_addr_t, in_port_t>, int> countMap;

	// Iterate over the heartbeats map to populate the countMap
	for (const auto &entry : heartbeats) {
		countMap[entry.second]++; // Increment count for each value
	}

	// Move the values and counts into a vector for sorting
	std::vector<std::pair<std::pair<in_addr_t, in_port_t>, int>> sortedCounts(countMap.begin(), countMap.end());

	// Sort the vector: first by count (descending), then by IP (ascending), and finally by port (ascending)
	std::sort(sortedCounts.begin(), sortedCounts.end(), [](const auto &a, const auto &b) {
		if (a.second != b.second) {
			return a.second > b.second; // Sort by count, descending
		}
		if (a.first.first != b.first.first) {
			return a.first.first < b.first.first; // Sort by IP, ascending
		}
		return a.first.second < b.first.second; // Sort by port, ascending
	});
	if (!sortedCounts.empty()) {
		const auto &result = sortedCounts.front().first;
		return {result.first, result.second}; // Return the IP and port pair
	}

	// Return default values if the heartbeats map is empty
	return {0, 0};
}

void MessageReceiver::handleFirstMessage(Request *request, int socketfd, bool broadcast) {
	if (broadcast && broadcastType == AB) {
		channelMessageIP = request->datagram->getSourceAddress();
		channelMessagePort = request->datagram->getDestinationPort();
		sendHEARTBEAT({channelMessageIP, channelMessagePort}, socketfd);
		channelOccupied = true;
	}
	if (messages.contains({request->datagram->getSourceAddress(), request->datagram->getDestinationPort()})) {
		sendDatagramSYNACK(request, socketfd);
	}
	auto *message = new Message(request->datagram->getDatagramTotal());
	if (broadcast) {
		message->broadcastMessage = true;
	}
	messages[{request->datagram->getSourceAddress(), request->datagram->getDestinationPort()}] = message;
	sendDatagramSYNACK(request, socketfd);
}

void MessageReceiver::deliverBroadcast(Message *message) {
	switch (broadcastType) {
	case AB:
		if (!message->allACK() || message->delivered)
			return;
		channelOccupied = false;
		message->delivered = true;
		messageQueue->push(std::make_pair(true, *message->getData()));
		break;
	case URB:
		if (!message->allACK() || message->delivered)
			return;
		message->delivered = true;
		messageQueue->push(std::make_pair(true, *message->getData()));
		break;
	default:
		if (message->delivered)
			return;
		message->delivered = true;
		messageQueue->push(std::make_pair(true, *message->getData()));
		break;
	}
}
void MessageReceiver::handleBroadcastDataMessage(Request *request, int socketfd) {
	std::shared_lock lock(messagesMutex);
	Message *message = getMessage(request->datagram);
	if (message == nullptr) {
		sendDatagramFIN(request, socketfd);
		return;
	}
	std::shared_lock messageLock(*message->getMutex());

	if (message->delivered) {
		sendDatagramFINACK(request, socketfd);
		return;
	}
	if (!message->verifyMessage(*request->datagram)) {
		return;
	}
	sendDatagramACK(request, socketfd);
	bool sent = message->addData(request->datagram);
	if (sent) {
		sendDatagramFINACK(request, socketfd);
	}
}


// Should be used with read lock.
Message *MessageReceiver::getMessage(Datagram *datagram) {

	std::pair identifier = {datagram->getSourceAddress(), datagram->getDestinationPort()};
	if (messages.find(identifier) == messages.end()) {
		return nullptr;
	}
	return messages[identifier];
}

bool MessageReceiver::sendDatagramACK(Request *request, int socketfd) {
	auto datagramACK = Datagram(request->datagram);
	datagramACK.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramACK.setSourceAddress(source.sin_addr.s_addr);
	datagramACK.setSourcePort(source.sin_port);

	Flags flags;
	flags.ACK = true;
	Protocol::setFlags(&datagramACK, &flags);
	return Protocol::sendDatagram(&datagramACK, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFIN(Request *request, int socketfd) {
	auto datagramFIN = Datagram(request->datagram);
	datagramFIN.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramFIN.setSourceAddress(source.sin_addr.s_addr);
	datagramFIN.setSourcePort(source.sin_port);
	Flags flags;
	flags.FIN = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(&datagramFIN, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFINACK(Request *request, int socketfd) {
	auto datagramFINACK = Datagram(request->datagram);
	datagramFINACK.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramFINACK.setSourceAddress(source.sin_addr.s_addr);
	datagramFINACK.setSourcePort(source.sin_port);
	Flags flags;
	flags.FIN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramFINACK, &flags);
	return Protocol::sendDatagram(&datagramFINACK, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramSYNACK(Request *request, int socketfd) {
	auto datagramSYNACK = Datagram(request->datagram);
	datagramSYNACK.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramSYNACK.setSourceAddress(source.sin_addr.s_addr);
	datagramSYNACK.setSourcePort(source.sin_port);

	Flags flags;
	flags.SYN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramSYNACK, &flags);
	bool sent = Protocol::sendDatagram(&datagramSYNACK, request->clientRequest, socketfd, &flags);
	return sent;
}

bool MessageReceiver::sendHEARTBEAT(std::pair<unsigned int, unsigned short> target, int socketfd) {
	auto heartbeat = Datagram();
	sockaddr_in source = configs->at(id);
	heartbeat.setSourceAddress(source.sin_addr.s_addr);
	heartbeat.setSourcePort(source.sin_port);
	heartbeat.setDestinAddress(target.first);
	heartbeat.setDestinationPort(target.second);

	Flags flags;
	flags.HEARTBEAT = true;
	Protocol::setFlags(&heartbeat, &flags);
	auto addr = Protocol::broadcastAddress();
	bool sent = Protocol::sendDatagram(&heartbeat, &addr, socketfd, &flags);
	return sent;
}
