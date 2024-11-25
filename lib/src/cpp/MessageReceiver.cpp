#include "../header/MessageReceiver.h"

#include <map>
#include <shared_mutex>
#include "BlockingQueue.h"
#include "BroadcastType.h"
#include "Datagram.h"
#include "Logger.h"
#include "NodeStatus.h"
#include "Protocol.h"
#include "Request.h"
#include "TypeUtils.h"


MessageReceiver::MessageReceiver(BlockingQueue<std::pair<bool, std::vector<unsigned char>>> *messageQueue,
								 DatagramController *datagramController, std::map<unsigned short, sockaddr_in> *configs,
								 unsigned short id, const BroadcastType &broadcastType, int broadcastFD,
								 StatusStruct *statusStruct, MessageSender *sender, int aliveMS) {
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
	this->sender = sender;
	this->aliveTimeMS = aliveMS;
}

MessageReceiver::~MessageReceiver() {
	{
		std::lock_guard lock(mtx);
		running = false;
	}
	messageQueue->push(std::make_pair(false, std::vector<unsigned char>()));
	running = false;
	cv.notify_all();
	// if (cleanseThread.joinable())
	// 	cleanseThread.join();
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


// TODO change res
void MessageReceiver::heartbeat() {
	while (running) {
		{
			sendHEARTBEAT({channelIP, channelPort}, broadcastFD);
			std::this_thread::sleep_for(std::chrono::milliseconds(aliveTimeMS));
			std::vector<std::pair<unsigned int, unsigned short>> removes;
			std::lock_guard lock(statusStruct->nodeStatusMutex);
			for (auto [identifier, time] : heartbeatsTimes) {
				auto oldStatus = statusStruct->nodeStatus[identifier];
				if (std::chrono::system_clock::now() - time >= std::chrono::milliseconds(3 * aliveTimeMS) &&
					statusStruct->nodeStatus[identifier] != NOT_INITIALIZED) {
					statusStruct->nodeStatus[identifier] = DEFECTIVE;
					// removes.emplace_back(identifier);
				}
				else if (std::chrono::system_clock::now() - time >= std::chrono::milliseconds(2 * aliveTimeMS) &&
						 statusStruct->nodeStatus[identifier] != NOT_INITIALIZED) {
					statusStruct->nodeStatus[identifier] = SUSPECT;
				}
				if (oldStatus != statusStruct->nodeStatus[identifier]) {
					Logger::log("New status for node " + std::to_string((*configs)[id].sin_port) + ": " +
									Protocol::getNodeStatusString(statusStruct->nodeStatus[identifier]),
								LogLevel::DEBUG);
				}
			}
			if (status == SYNCHRONIZE && statusStruct->nodeStatus[{channelIP, channelPort}] != NOT_INITIALIZED) {
				Logger::log("Not synchronizing anymore", LogLevel::DEBUG);
				status = INITIALIZED;
			}
		}
	}
}

void MessageReceiver::synchronize() {
	synchronizing = true;
	for (auto message : broadcastOrder) {
		if (message->delivered) {
			for (auto [identifier, m] : broadcastMessages) {
				if (m == message) {
					if (!message->delivered)
						continue;
					auto data = *message->getData();
					sender->synchronizeBroadcast(data, identifier, {channelIP, channelPort}, m->origin);
				}
			}
		}
	}
	synchronizing = false;
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

void MessageReceiver::treatHeartbeat(Datagram *datagram) {
	unsigned statusInt = TypeUtils::buffToUnsignedInt(*datagram->getData(), 0);
	NodeStatus status = Protocol::getNodeStatus(statusInt);
	std::lock_guard lock(statusStruct->nodeStatusMutex);
	auto oldStatus = statusStruct->nodeStatus[{datagram->getSourceAddress(), datagram->getSourcePort()}];

	if (oldStatus != status) {
		Logger::log("New status for node " + std::to_string(datagram->getSourcePort()) + ": " +
						Protocol::getNodeStatusString(status),
					LogLevel::DEBUG);
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

	{
		std::shared_lock statusLock(statusStruct->nodeStatusMutex);
		if (status == SYNCHRONIZE && statusStruct->nodeStatus[{channelIP, channelPort}] != NOT_INITIALIZED) {
			status = INITIALIZED;
		}
	}

	// É uma mensagem mas o nó ainda não esta inicializado
	if (datagram->isSYNCHRONIZE() && status == NOT_INITIALIZED)
		return;
	// Recebeu concordância com join
	if (datagram->isJOIN() && datagram->isACK()) {
		datagramController->createQueue({datagram->getDestinAddress(), datagram->getDestinationPort()});
		datagramController->insertDatagram({datagram->getDestinAddress(), datagram->getDestinationPort()}, datagram);
		return;
	}

	if (datagram->isJOIN() && status != NOT_INITIALIZED) {
		// Someone is already entering the channel
		if (status == SYNCHRONIZE &&
			(datagram->getSourceAddress() != channelIP || datagram->getSourcePort() != channelPort)) {

			std::shared_lock statusLock(statusStruct->nodeStatusMutex);
			// Se o nó em sincronização morreu substitui o nó
			if (statusStruct->nodeStatus[{channelIP, channelPort}] == NOT_INITIALIZED) {
				return;
			}
		}
		channelIP = datagram->getSourceAddress();
		channelPort = datagram->getSourcePort();
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
		status = SYNCHRONIZE;
		// Não há thread dedicada para recepção.
		if (!synchronizing) {
			synchronyzeThread = std::thread([this] { synchronize(); });
			synchronyzeThread.detach();
		}
		return;
	}

	// Não possui nenhuma mensagem esperada
	if (broadcastType == AB && status == RECEIVING) {
		// Cria uma mensagem mesmo que não vá ser utilizada para posterior consenso
		if (datagram->getFlags() == 2) {
			createMessage(request, broadcastFD);
		}
		// Caso haja alguma mensagem esperada
		std::pair messageID = {request->datagram->getDestinAddress(), request->datagram->getDestinationPort()};
		// Se a mensagem recebida for diferente da esperada
		if (channelIP != messageID.first || channelPort != messageID.second) {
			std::pair<unsigned int, unsigned short> channelMessageID = {channelIP, channelPort};
			std::pair consent = verifyConsensus();
			// Consenso espera alguma coisa
			if ((consent.first != 0 || consent.second != 0) &&
				(consent.first != channelMessageID.first || consent.second != channelMessageID.second)) {
				return;
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
	if (datagram->isSYN()) {
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
	if (!messages.contains({request->datagram->getDestinAddress(), request->datagram->getDestinationPort()}) &&
		!broadcastMessages.contains({request->datagram->getDestinAddress(), request->datagram->getDestinationPort()})) {
		auto *message = new Message(request->datagram->getDatagramTotal());
		message->origin = {request->datagram->getSourceAddress(), request->datagram->getSourcePort()};
		if (broadcast) {
			message->broadcastMessage = true;
			broadcastMessages[{request->datagram->getDestinAddress(), request->datagram->getDestinationPort()}] =
				message;
			broadcastOrder.emplace_back(message);
			return;
		}
		messages[{request->datagram->getDestinAddress(), request->datagram->getDestinationPort()}] = message;
	}
}
void MessageReceiver::handleFirstMessage(Request *request, int socketfd, bool broadcast) {
	if (messages.contains({request->datagram->getDestinAddress(), request->datagram->getDestinationPort()}) &&
		!broadcastMessages.contains({request->datagram->getDestinAddress(), request->datagram->getDestinationPort()})) {
		sendDatagramSYNACK(request, socketfd);
		return;
	}
	if (broadcast && broadcastType == AB) {
		channelIP = request->datagram->getDestinAddress();
		channelPort = request->datagram->getDestinationPort();
		sendHEARTBEAT({channelIP, channelPort}, socketfd);
		if (status != NOT_INITIALIZED && status != SYNCHRONIZE)
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
		if (status != NOT_INITIALIZED && status != DEFECTIVE) {
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
//

void MessageReceiver::deliverBroadcast(Message *message, int broadcastfd) {
	switch (broadcastType) {
	case AB:
		if (!message->messageACK())
			return;
		if (status != SYNCHRONIZE && status != NOT_INITIALIZED)
			status = INITIALIZED;
		for (auto m : broadcastOrder) {
			if (!m->sent)
				break;
			if (m->sent && !m->delivered) {
				m->delivered = true;
				sendHEARTBEAT({0, 0}, broadcastfd);
				messageQueue->push(std::make_pair(true, *m->getData()));
			}
		}
		break;
	case URB:
		if (!message->messageACK() || message->delivered)
			return;
		if (status != SYNCHRONIZE && status != NOT_INITIALIZED)
			status = INITIALIZED;
		message->delivered = true;
		messageQueue->push(std::make_pair(true, *message->getData()));
		break;
	default:
		if (message->delivered)
			return;
		if (status != SYNCHRONIZE && status != NOT_INITIALIZED)
			status = INITIALIZED;
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

unsigned MessageReceiver::getBroadcastSize() {
	unsigned size = 0;
	for (auto message : broadcastOrder) {
		if (message->delivered)
			size++;
	}
	return size;
}

bool MessageReceiver::sendDatagramJOINACK(Request *request, int socketfd) {
	auto datagramJOINACK = Datagram(request->datagram);
	datagramJOINACK.setVersion(request->datagram->getVersion());
	datagramJOINACK.setDestinAddress(datagramJOINACK.getSourceAddress());
	datagramJOINACK.setDestinationPort(datagramJOINACK.getSourcePort());
	sockaddr_in source = configs->at(id);
	auto buff = std::vector<unsigned char>();
	buff.resize(4);
	TypeUtils::uintToBytes(getBroadcastSize(), &buff);
	datagramJOINACK.setData(buff);
	datagramJOINACK.setDataLength(4);
	datagramJOINACK.setSourceAddress(source.sin_addr.s_addr);
	datagramJOINACK.setSourcePort(source.sin_port);

	if (getBroadcastSize() != 0) {
		status = SYNCHRONIZE;
	}
	Flags flags;
	flags.JOIN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramJOINACK, &flags);
	auto broadcastaddr = Protocol::broadcastAddress();
	bool sent = Protocol::sendDatagram(&datagramJOINACK, &broadcastaddr, socketfd, &flags);
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

void MessageReceiver::configure() {
	status = INITIALIZED; // TODO send heartbeat
}

unsigned MessageReceiver::getBroadcastMessagesSize() {
	unsigned delivered = 0;
	for (auto message : broadcastOrder) {
		if (message->delivered)
			delivered++;
	}
	return delivered;
}
