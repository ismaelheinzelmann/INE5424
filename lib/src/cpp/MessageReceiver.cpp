#include "../header/MessageReceiver.h"

#include <cassert>
#include <iostream>
#include "Logger.h"
#include "Request.h"

#include <map>
#include <shared_mutex>
#include "BlockingQueue.h"
#include "Datagram.h"
#include "Protocol.h"

MessageReceiver::MessageReceiver(BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue,
								 DatagramController *datagramController) {
	this->messages = std::map<std::pair<in_addr_t, in_port_t>, Message *>();
	this->messageQueue = messageQueue;
	this->datagramController = datagramController;
	cleanseThread = std::thread([this] { cleanse(); });
	cleanseThread.detach();
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
	if (!verifyMessage(request)) {
		sendDatagramNACK(request, socketfd);
		return;
	}

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

void MessageReceiver::handleFirstMessage(Request *request, int socketfd) {
	auto *message = new Message(request->datagram->getDatagramTotal());
	std::lock_guard lock(messagesMutex);
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
	if (!message->verifyMessage(*(request->datagram))) {
		sendDatagramNACK(request, socketfd);
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
	Flags flags;
	flags.ACK = true;
	Protocol::setFlags(&datagramACK, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramNACK(Request *request, int socketfd) {
	auto datagramNACK = Datagram();
	datagramNACK.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.NACK = true;
	Protocol::setFlags(&datagramNACK, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFIN(Request *request, int socketfd) {
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.FIN = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFINACK(Request *request, int socketfd) {
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.FIN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramSYNACK(Request *request, int socketfd) {
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.SYN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}
