#include "../header/MessageReceiver.h"

#include <iostream>
#include <map>
#include <shared_mutex>
#include "BlockingQueue.h"
#include "Datagram.h"
#include "Logger.h"
#include "Protocol.h"
#include "Request.h"

MessageReceiver::MessageReceiver(BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue,
								 DatagramController *datagramController,
								 std::map<unsigned short, sockaddr_in> *configs, unsigned short id) {
	this->messages = std::map<std::pair<in_addr_t, in_port_t>, Message *>();
	this->messageQueue = messageQueue;
	this->datagramController = datagramController;
	cleanseThread = std::thread([this] { cleanse(); });
	cleanseThread.detach();
	this->configs = configs;
	this->id = id;
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
	Logger::log("Cleanse thread initialized.", LogLevel::DEBUG);
	while (true) {
		{
			std::unique_lock lock(mtx);
			Logger::log("Cleanse thread sleeping", LogLevel::DEBUG);
			if (cv.wait_for(lock, std::chrono::seconds(10), [this] { return !running; })) {
				Logger::log("Cleanse thread ended.", LogLevel::DEBUG);
				return;
			}
		}
		Logger::log("Cleanse thread running", LogLevel::DEBUG);
		{
			std::lock_guard messagesLock(messagesMutex);
			for (auto it = this->messages.begin(); it != this->messages.end();) {
				auto &[pair, message] = *it;
				if (std::chrono::system_clock::now() - message->getLastUpdate() > std::chrono::seconds(10)) {
					std::lock_guard messageLock(*message->getMutex());
					delete message;
					it = messages.erase(it);
				}
				else {
					++it;
				}
			}
		}
	}
}


bool MessageReceiver::verifyMessage(Request *request) {
	return Protocol::verifyChecksum(request->datagram, request->data);
}

void MessageReceiver::handleMessage(Request *request, int socketfd) {
	std::shared_lock lock(messagesMutex);
	if (!verifyMessage(request)) {
		sendDatagramNACK(request, socketfd);
		return;
	}
	Message *message = getMessage(request->datagram);
	if (message == nullptr)
		return;

	// Data datagram
	if (request->datagram->getFlags() == 0) {
		handleDataMessage(request, socketfd);
		return;
	}
	if ((request->datagram->isSYN() && request->datagram->isACK()) ||
		(request->datagram->isFIN() && request->datagram->isACK()) || request->datagram->isACK() ||
		request->datagram->isNACK() || request->datagram->isFIN()) {

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
		sendDatagramNACK(request, socketfd);
		return;
	}
	Protocol::setBroadcast(request);
	// Data datagram
	if (request->datagram->getFlags() == 0) {
		handleDataMessage(request, socketfd);
		return;
	}
	if ((request->datagram->isSYN() && request->datagram->isACK()) ||
		(request->datagram->isFIN() && request->datagram->isACK()) || request->datagram->isACK() ||
		request->datagram->isNACK() || request->datagram->isFIN()) {
		Message *message = getMessage(request->datagram);
		if (message == nullptr)
			return;
		if (request->datagram->isFIN() && request->datagram->isACK()) {
			std::shared_lock messageLock(*message->getMutex());
			if (const std::pair identifier = {request->datagram->getSourceAddress(),
											  request->datagram->getDestinationPort()};
				message->acks.contains(identifier))
				message->acks[identifier] = true;
		}
		datagramController->insertDatagram(
			{request->datagram->getSourceAddress(), request->datagram->getDestinationPort()}, request->datagram);
		return;
	}
	if (request->datagram->isSYN()) {
		handleFirstMessage(request, socketfd, true);
	}
}

void MessageReceiver::handleFirstMessage(Request *request, int socketfd, bool broadcast) {
	// std::lock_guard lock(messagesMutex);
	if (messages.contains({request->datagram->getSourceAddress(), request->datagram->getDestinationPort()})) {
		sendDatagramSYNACK(request, socketfd);
	}
	auto *message = new Message(request->datagram->getDatagramTotal());
	if (broadcast) {
		message->broadcastMessage = true;
		for (auto [_, config] : *configs) {
			message->acks[{config.sin_addr.s_addr, config.sin_port}] = false;
		}
	}
	messages[{request->datagram->getSourceAddress(), request->datagram->getDestinationPort()}] = message;
	sendDatagramSYNACK(request, socketfd);
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
	if (!message->verifyMessage(*request->datagram)) {
		sendDatagramNACK(request, socketfd);
		return;
	}
	bool sent = message->addData(request->datagram);
	if (sent) {
		sendDatagramFINACK(request, socketfd);
		if (!message->delivered) {
			if (false) { // IF URB
				if (message->allACK()) {
					message->delivered = true;
					messageQueue->push(std::make_pair(true, *message->getData()));
				}
			}
			else {
				message->delivered = true;
				messageQueue->push(std::make_pair(true, *message->getData()));
			}
		}
	}
	sendDatagramACK(request, socketfd);
}


// Should be used with read lock.
Message *MessageReceiver::getMessage(Datagram *datagram) {

	std::pair<in_addr_t, in_port_t> identifier = {datagram->getSourceAddress(), datagram->getDestinationPort()};
	if (messages.find(identifier) == messages.end()) {
		return nullptr;
	}
	return messages[identifier];
}

bool MessageReceiver::sendDatagramACK(Request *request, int socketfd) {
	auto datagramACK = Datagram();
	datagramACK.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramACK.setSourceAddress(source.sin_addr.s_addr);
	datagramACK.setSourcePort(source.sin_port);

	Flags flags;
	flags.ACK = true;
	Protocol::setFlags(&datagramACK, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramNACK(Request *request, int socketfd) {
	auto datagramNACK = Datagram();
	datagramNACK.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramNACK.setSourceAddress(source.sin_addr.s_addr);
	datagramNACK.setSourcePort(source.sin_port);
	Flags flags;
	flags.NACK = true;
	Protocol::setFlags(&datagramNACK, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFIN(Request *request, int socketfd) {
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramFIN.setSourceAddress(source.sin_addr.s_addr);
	datagramFIN.setSourcePort(source.sin_port);
	Flags flags;
	flags.FIN = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFINACK(Request *request, int socketfd) {
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramFIN.setSourceAddress(source.sin_addr.s_addr);
	datagramFIN.setSourcePort(source.sin_port);
	Flags flags;
	flags.FIN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramSYNACK(Request *request, int socketfd) {
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	sockaddr_in source = configs->at(id);
	datagramFIN.setSourceAddress(source.sin_addr.s_addr);
	datagramFIN.setSourcePort(source.sin_port);

	Flags flags;
	flags.SYN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}
