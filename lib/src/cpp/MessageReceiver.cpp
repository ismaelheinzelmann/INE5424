#include "../header/MessageReceiver.h"

#include <NodeStatus.h>
#include <TypeUtils.h>
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
								 unsigned short id, const BroadcastType &broadcastType, int broadcastFD,
								 StatusStruct *statusStruct) {
	this->messages = std::map<std::pair<in_addr_t, in_port_t>, Message *>();
	this->messageQueue = messageQueue;
	this->datagramController = datagramController;
	// cleanseThread = std::thread([this] { cleanse(); });
	// cleanseThread.detach();
	heartbeatThread = std::thread([this] { heartbeat(); });
	heartbeatThread.detach();
	this->configs = configs;
	this->id = id;
	this->broadcastType = broadcastType;
	this->broadcastFD = broadcastFD;
	this->statusStruct = statusStruct;
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
	if (heartbeatThread.joinable())
		heartbeatThread.join();
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


// void MessageReceiver::cleanse() {
// 	while (true) {
// 		{
// 			std::unique_lock lock(mtx);
// 			if (cv.wait_for(lock, std::chrono::seconds(10), [this] { return !running; })) {
// 				return;
// 			}
// 		}
// 		{
// 			std::lock_guard messagesLock(messagesMutex);
// 			auto remove = std::vector<std::pair<unsigned int, unsigned short>>();
// 			for (auto [identifier, message] : messages) {
// 				if (std::chrono::system_clock::now() - message->getLastUpdate() > std::chrono::seconds(10)) {
// 					remove.emplace_back(identifier);
// 				}
// 			}
// 			for (auto identifier : remove) {
// 				this->messages.erase(identifier);
// 				datagramController->deleteQueue(identifier);
// 			}
// 		}
// 	}
// }
// TODO change res
void MessageReceiver::heartbeat() {
	while (running) {
		{
			if (status == NOT_INITIALIZED) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
			sendHEARTBEAT({channelIP, channelPort}, broadcastFD);
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::vector<std::pair<unsigned int, unsigned short>> removes;
			std::lock_guard lock(statusStruct->nodeStatusMutex);
			for (auto [identifier, time] : heartbeatsTimes) {
				auto oldStatus = statusStruct->nodeStatus[identifier];
				if (std::chrono::system_clock::now() - time >= std::chrono::seconds(3) &&
					statusStruct->nodeStatus[identifier] != NOT_INITIALIZED) {
					statusStruct->nodeStatus[identifier] = DEFECTIVE;
					// removes.emplace_back(identifier);
				}
				else if (std::chrono::system_clock::now() - time >= std::chrono::seconds(2) &&
						 statusStruct->nodeStatus[identifier] != NOT_INITIALIZED) {
					statusStruct->nodeStatus[identifier] = SUSPECT;
				}
				if (oldStatus != statusStruct->nodeStatus[identifier]) {
					Logger::log("New status for node: " +
									Protocol::getNodeStatusString(statusStruct->nodeStatus[identifier]),
								LogLevel::DEBUG);
				}
			}
			// for (auto identifier : removes) {
			// 	if (heartbeats.count(identifier))
			// 		heartbeats.erase(identifier);
			// 	if (heartbeatsTimes.count(identifier))
			// 		heartbeatsTimes.erase(identifier);
			// }
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

	// Data datagram
	if (request->datagram->getFlags() == 0) {
		handleDataMessage(request, socketfd);
		return;
	}
	if ((request->datagram->isSYN() && request->datagram->isACK()) ||
		(request->datagram->isFIN() && request->datagram->isACK()) || request->datagram->isACK() ||
		request->datagram->isFIN()) {
		datagramController->insertDatagram(
			{request->datagram->getDestinAddress(), request->datagram->getDestinationPort()}, request->datagram);
		return;
	}
	if (request->datagram->isSYN()) {
		handleFirstMessage(request, socketfd);
	}
}

// Se for um JOIN e o canal não estiver SYNCHRONIZE, pode dale (menos criar uma nova mensagem)
// De qualquer modo, verificar se o mesmo que tem que enviar as mensagenss

void MessageReceiver::treatHeartbeat(Datagram *datagram) {
	unsigned statusInt = TypeUtils::buffToUnsignedInt(*datagram->getData(), 0);
	NodeStatus status = Protocol::getNodeStatus(statusInt);
	std::lock_guard lock(statusStruct->nodeStatusMutex);
	auto oldStatus = statusStruct->nodeStatus[{datagram->getSourceAddress(), datagram->getSourcePort()}];

	if (oldStatus != status) {
		Logger::log("New status for node: " + Protocol::getNodeStatusString(statusInt), LogLevel::DEBUG);
	}
	(statusStruct->nodeStatus)[{datagram->getSourceAddress(), datagram->getSourcePort()}] = status;
}

void MessageReceiver::handleBroadcastMessage(Request *request) {
	std::shared_lock lock(messagesMutex);
	Protocol::setBroadcast(request);
	Datagram *datagram = request->datagram;
	if (datagram->isHEARTBEAT()) {
		std::pair identifier = {datagram->getSourceAddress(), datagram->getSourcePort()};
		std::unique_lock hblock(heartbeatsLock);
		heartbeats[identifier] = {datagram->getDestinAddress(), datagram->getDestinationPort()};
		heartbeatsTimes[identifier] = std::chrono::system_clock::now();
		treatHeartbeat(request->datagram);
		return;
	}
	// É uma mensagem mas o nó ainda não esta inicializado
	if ((datagram->isJOIN() || datagram->isSYNCHRONIZE()) && status == NOT_INITIALIZED)
		return;
	// Recebeu concordância com join
	if (datagram->isJOIN() && datagram->isACK()) {
		datagramController->insertDatagram({datagram->getDestinAddress(), datagram->getDestinationPort()}, datagram);
		return;
	}
	if (datagram->isJOIN()) {
		// Is a self message
		if (datagram->getSourceAddress() == configs->at(id).sin_addr.s_addr &&
			datagram->getSourcePort() == configs->at(id).sin_port)
			return;
		// Someone is already entering the channel
		if (status == SYNCHRONIZE && datagram->getSourceAddress() != channelIP &&
			datagram->getSourcePort() != channelPort) {
			return;
		}
		sendDatagramJOINACK(request, broadcastFD);
		return;
	}
	if (datagram->isSYNCHRONIZE()) {
		auto smallestProcess = getSmallestProcess();
		// Não há processo configurado
		if (smallestProcess.first == 0 || smallestProcess.second == 0)
			return;
		// Este processo não deve responder a solicitação, pois não é o menor.
		if (smallestProcess.first != configs->at(id).sin_addr.s_addr &&
			smallestProcess.second != configs->at(id).sin_port) {
			return;
		}
		// TODO Fazer método de send heartbeat onde ele ja faz sozinha a parte de construir tudo e pegar o estado.
	}

	Message *message = getBroadcastMessage(request->datagram);
	if (message != nullptr && message->delivered && request->datagram->getFlags() == 0) {
		sendDatagramFINACK(request, broadcastFD);
		return;
	}

	// Não possui nenhuma mensagem esperada
	if (broadcastType == AB) {
		// Cria uma mensagem mesmo que não vá ser utilizada para posterior consenso
		if (datagram->getFlags() == 2) {
			createMessage(request, broadcastFD);
		}
		// Caso haja alguma mensagem esperada
		if (status == RECEIVING) {
			std::pair messageID = {request->datagram->getDestinAddress(), request->datagram->getDestinationPort()};
			// Se a mensagem recebida for diferente da esperada
			if (channelIP != messageID.first || channelPort != messageID.second) {
				std::pair<unsigned int, unsigned short> channelMessageID = {channelIP, channelPort};
				std::pair consent = verifyConsensus();
				// Consenso espera alguma coisa
				if ((consent.first != 0 || consent.second != 0) &&
					(consent.first != channelMessageID.first || consent.second != channelMessageID.second)) {
					if (messages.contains(consent)) {
						std::shared_lock messageLock(*messages[consent]->getMutex());
						// Mensagem anterior não foi finalizada e recebeu um timeout, logo substitui a mensagem aceita.
						if (!messages[consent]->delivered &&
							messages[consent]->getLastUpdate() - std::chrono::system_clock::now() >
								std::chrono::seconds(3)) {

							channelIP = consent.first;
							channelPort = consent.second;
						}
						else {
							// Mensagem anterior não foi finalizada, porém ainda não tomou timeout
							return;
						}
					}
				}
			}
		}
	}

	// Data datagram
	if (datagram->getFlags() == 0) {
		handleBroadcastDataMessage(request);
		return;
	}
	if ((datagram->isSYN() && datagram->isACK()) || (datagram->isFIN() && datagram->isACK()) || datagram->isACK() ||
		datagram->isFIN()) {
		std::shared_lock lock(messagesMutex);
		Message *message = getBroadcastMessage(datagram);
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
					deliverBroadcast(message, broadcastFD);
				}
			}
		}
		datagramController->insertDatagram({datagram->getDestinAddress(), datagram->getDestinationPort()}, datagram);
		return;
	}
	if (datagram->isSYN() && (status == INITIALIZED || status == NOT_INITIALIZED)) {
		handleFirstMessage(request, broadcastFD, true);
	}
}

std::pair<unsigned int, unsigned short> MessageReceiver::verifyConsensus() {
	std::map<std::pair<in_addr_t, in_port_t>, int> countMap;

	for (const auto &entry : heartbeats) {
		countMap[entry.second]++;
	}

	std::vector<std::pair<std::pair<in_addr_t, in_port_t>, int>> sortedCounts(countMap.begin(), countMap.end());

	std::sort(sortedCounts.begin(), sortedCounts.end(), [](const auto &a, const auto &b) {
		if (a.second != b.second) {
			return a.second > b.second;
		}
		if (a.first.first != b.first.first) {
			return a.first.first < b.first.first;
		}
		return a.first.second < b.first.second;
	});
	if (!sortedCounts.empty()) {
		const auto &result = sortedCounts.front().first;
		return {result.first, result.second};
	}

	return {0, 0};
}

void MessageReceiver::createMessage(Request *request, bool broadcast) {
	if (!messages.contains({request->datagram->getDestinAddress(), request->datagram->getDestinationPort()})) {
		auto *message = new Message(request->datagram->getDatagramTotal());
		if (broadcast) {
			message->broadcastMessage = true;
			broadcastMessages[{request->datagram->getDestinAddress(), request->datagram->getDestinationPort()}] =
				message;
			if (broadcastType == AB)
				broadcastOrder.emplace_back(message);
			return;
		}
		messages[{request->datagram->getDestinAddress(), request->datagram->getDestinationPort()}] = message;
	}
}
void MessageReceiver::handleFirstMessage(Request *request, int socketfd, bool broadcast) {
	if (broadcast && broadcastType == AB) {
		channelIP = request->datagram->getDestinAddress();
		channelPort = request->datagram->getDestinationPort();
		sendHEARTBEAT({channelIP, channelPort}, socketfd);
		status = RECEIVING;
	}
	createMessage(request, broadcast);
	sendDatagramSYNACK(request, socketfd);
}

std::pair<unsigned, unsigned short> MessageReceiver::getSmallestProcess() {
	std::pair<unsigned, unsigned short> smallestPair;
	unsigned smallestIp = std::numeric_limits<unsigned>::max();
	unsigned short smallestPort = std::numeric_limits<unsigned short>::max();
	bool found = false;

	// Iterar sobre o mapa
	std::shared_lock lock(statusStruct->nodeStatusMutex);
	for (const auto &[key, status] : statusStruct->nodeStatus) {
		if (status == INITIALIZED) {
			const auto &[ip, port] = key;
			if (ip < smallestIp || (ip == smallestIp && port < smallestPort)) {
				smallestIp = ip;
				smallestPort = port;
				smallestPair = key;
				found = true;
			}
		}
	}
	if (!found) {
		return {0, 0};
	}
	return smallestPair;
}

void MessageReceiver::deliverBroadcast(Message *message, int broadcastfd) {
	switch (broadcastType) {
	case AB:
		if (!message->allACK())
			return;
		status = INITIALIZED;
		message->delivered = true;
		sendHEARTBEAT({0, 0}, broadcastfd);
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
void MessageReceiver::handleBroadcastDataMessage(Request *request) {
	std::shared_lock lock(messagesMutex);
	Message *message = getBroadcastMessage(request->datagram);
	if (message == nullptr) {
		sendDatagramFIN(request, broadcastFD);
		return;
	}
	std::shared_lock messageLock(*message->getMutex());

	if (message->delivered || message->sent) {
		sendDatagramFINACK(request, broadcastFD);
		return;
	}
	if (!message->verifyMessage(*request->datagram)) {
		return;
	}
	sendDatagramACK(request, broadcastFD);
	bool sent = message->addData(request->datagram);
	if (sent) {
		sendDatagramFINACK(request, broadcastFD);
	}
}


// Should be used with read lock.
Message *MessageReceiver::getMessage(Datagram *datagram) {

	std::pair identifier = {datagram->getDestinAddress(), datagram->getDestinationPort()};
	if (messages.find(identifier) == messages.end()) {
		return nullptr;
	}
	return messages[identifier];
}

Message *MessageReceiver::getBroadcastMessage(Datagram *datagram) {

	std::pair identifier = {datagram->getDestinAddress(), datagram->getDestinationPort()};
	if (broadcastMessages.find(identifier) == broadcastMessages.end()) {
		return nullptr;
	}
	return broadcastMessages[identifier];
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

bool MessageReceiver::sendDatagramJOINACK(Request *request, int socketfd) {
	auto datagramJOINACK = Datagram(request->datagram);
	datagramJOINACK.setVersion(request->datagram->getVersion());
	datagramJOINACK.setDestinAddress(datagramJOINACK.getSourceAddress());
	datagramJOINACK.setDestinationPort(datagramJOINACK.getSourcePort());
	sockaddr_in source = configs->at(id);
	auto buff = std::vector<unsigned char>();
	buff.resize(4);
	TypeUtils::uintToBytes(this->broadcastOrder.size(), &buff);
	datagramJOINACK.setData(buff);
	datagramJOINACK.setSourceAddress(source.sin_addr.s_addr);
	datagramJOINACK.setSourcePort(source.sin_port);

	Flags flags;
	flags.JOIN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramJOINACK, &flags);
	bool sent = Protocol::sendDatagram(&datagramJOINACK, request->clientRequest, socketfd, &flags);
	return sent;
}

bool MessageReceiver::sendHEARTBEAT(std::pair<unsigned int, unsigned short> target, int socketfd) {
	auto heartbeat = Datagram();
	sockaddr_in source = configs->at(id);
	auto buff = std::vector<unsigned char>();
	buff.resize(4);
	TypeUtils::uintToBytes(status, &buff);
	heartbeat.setData(buff);
	heartbeat.setDataLength(4);
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

void MessageReceiver::configure() { status = INITIALIZED; }

unsigned MessageReceiver::getBroadcastMessagesSize() { return broadcastMessages.size();
}
