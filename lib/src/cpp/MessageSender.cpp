#include "../header/MessageSender.h"
#include <cmath>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "../header/BlockingQueue.h"
#include "../header/ConfigParser.h"
#include "../header/Logger.h"
#include "../header/Protocol.h"

#include <cstring>
#include <random>
#include <thread>

#define RETRY_ACK_ATTEMPT 6
#define RETRY_ACK_TIMEOUT_USEC 50
#define RETRY_DATA_ATTEMPT 8
#define BATCH_SIZE 100

MessageSender::MessageSender(int socketFD, int broadcastFD, sockaddr_in configIdAddr,
							 DatagramController *datagramController, std::map<unsigned short, sockaddr_in> *configMap,
							 BroadcastType broadcastType, StatusStruct *statusStruct) {
	this->socketFD = socketFD;
	this->broadcastFD = broadcastFD;
	this->datagramController = datagramController;
	this->configAddr = configIdAddr;
	this->configMap = configMap;
	this->broadcastType = broadcastType;
	this->statusStruct = statusStruct;
}

void MessageSender::buildDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
								   std::map<unsigned short, bool> *acknowledgments, in_port_t transientPort,
								   unsigned short totalDatagrams, std::vector<unsigned char> &message) {
	for (int i = 0; i < totalDatagrams; ++i) {
		auto versionDatagram = Datagram();
		versionDatagram.setSourceAddress(configAddr.sin_addr.s_addr);
		versionDatagram.setSourcePort(configAddr.sin_port);
		versionDatagram.setDestinationPort(transientPort);
		versionDatagram.setDestinAddress(configAddr.sin_addr.s_addr);
		versionDatagram.setVersion(i + 1);
		versionDatagram.setDatagramTotal(totalDatagrams);
		for (unsigned short j = 0; j < 2024; j++) {
			const unsigned int index = i * 2024 + j;
			if (index >= message.size())
				break;
			versionDatagram.getData()->push_back(message.at(index));
		}
		versionDatagram.setDataLength(versionDatagram.getData()->size());
		auto serializedDatagram = Protocol::serialize(&versionDatagram);
		Protocol::setChecksum(&serializedDatagram);
		(*datagrams)[i] = serializedDatagram;
		(*acknowledgments)[i] = false;
	}
}

void MessageSender::buildBroadcastDatagrams(
	std::vector<std::vector<unsigned char>> *datagrams,
	std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
	in_port_t transientPort, unsigned short totalDatagrams, std::vector<unsigned char> &message,
	std::map<std::pair<unsigned int, unsigned short>, bool> *members) {
	std::map<unsigned short, bool> acknowledgments, responses;
	{
		std::shared_lock lock(statusStruct->nodeStatusMutex);
		for (auto [identifier, nodeStatus] : statusStruct->nodeStatus) {
			if (nodeStatus != NOT_INITIALIZED && nodeStatus != DEFECTIVE)
				(*members)[identifier] = false;
		}
	}

	for (int i = 0; i < totalDatagrams; ++i) {
		auto versionDatagram = Datagram();
		versionDatagram.setSourceAddress(configAddr.sin_addr.s_addr);
		versionDatagram.setSourcePort(configAddr.sin_port);
		versionDatagram.setDestinationPort(transientPort);
		versionDatagram.setDestinAddress(configAddr.sin_addr.s_addr);
		versionDatagram.setVersion(i + 1);
		versionDatagram.setDatagramTotal(totalDatagrams);
		for (unsigned short j = 0; j < 2024; j++) {
			const unsigned int index = i * 2024 + j;
			if (index >= message.size())
				break;
			versionDatagram.getData()->push_back(message.at(index));
		}
		versionDatagram.setDataLength(versionDatagram.getData()->size());
		auto serializedDatagram = Protocol::serialize(&versionDatagram);
		Protocol::setChecksum(&serializedDatagram);
		(*datagrams)[i] = serializedDatagram;
		acknowledgments[i] = false;
		responses[i] = false;
		for (const auto &member : *members) {
			(*membersAcks)[{member.first.first, member.first.second}][i] = {false, false};
		}
	}
}

bool MessageSender::sendMessage(sockaddr_in &destin, std::vector<unsigned char> &message) {
	std::pair<int, sockaddr_in> transientSocketFd = createUDPSocketAndGetPort();
	auto datagram = Datagram();
	unsigned short totalDatagrams = calculateTotalDatagrams(message.size());
	datagram.setDatagramTotal(totalDatagrams);
	datagram.setSourceAddress(configAddr.sin_addr.s_addr);
	datagram.setSourcePort(configAddr.sin_port);
	datagram.setDestinationPort(transientSocketFd.second.sin_port);
	datagram.setDestinAddress(configAddr.sin_addr.s_addr);
	datagramController->createQueue({configAddr.sin_addr.s_addr, transientSocketFd.second.sin_port});

	bool accepted = ackAttempts(destin, &datagram);
	if (!accepted) {
		return false;
	}
	// build of datagrams
	std::vector<std::vector<unsigned char>> datagrams = std::vector<std::vector<unsigned char>>(totalDatagrams);
	std::map<unsigned short, bool> acknowledgments;
	buildDatagrams(&datagrams, &acknowledgments, transientSocketFd.second.sin_port, totalDatagrams, message);

	unsigned short batchSize = BATCH_SIZE, sent = 0, acks = 0;
	const double batchCount = static_cast<int>(ceil(static_cast<double>(totalDatagrams) / batchSize));
	std::vector<unsigned char> buff = std::vector<unsigned char>(2048);
	for (unsigned short batchStart = 0; batchStart < batchCount; batchStart++) {
		unsigned short batchIndex, batchAck = 0;
		if (sent == totalDatagrams) {
			close(transientSocketFd.first);
			return true;
		}
		for (int attempt = 0; attempt < RETRY_DATA_ATTEMPT; attempt++) {
			if (batchAck == batchSize || acks == totalDatagrams)
				break;

			for (unsigned short j = 0; j < batchSize; j++) {
				batchIndex = batchStart * batchSize + j;
				if (batchIndex >= totalDatagrams)
					break;
				if (acknowledgments[batchIndex])
					continue;
				sendto(socketFD, datagrams[batchIndex].data(), datagrams[batchIndex].size(), 0,
					   reinterpret_cast<sockaddr *>(&destin), sizeof(destin));
			}
			while (true) {
				Datagram *response = datagramController->getDatagramTimeout(
					{datagram.getSourceAddress(), datagram.getDestinationPort()}, RETRY_ACK_TIMEOUT_USEC);
				if (response == nullptr)
					break;
				// Resposta de outro batch, pode ser descartada.
				if (response->getVersion() - 1 < batchStart * batchSize ||
					response->getVersion() - 1 > (batchStart * batchSize) + batchSize) {
					Logger::log("Received old response.", LogLevel::DEBUG);
					continue;
				}
				// Armazena informação de ACK recebido.
				if (response->getVersion() - 1 <= totalDatagrams && response->isACK() &&
					!acknowledgments[response->getVersion() - 1]) {
					Logger::log("Datagram of version " + std::to_string(response->getVersion()) + " accepted.",
								LogLevel::DEBUG);
					acknowledgments[response->getVersion() - 1] = true;
					batchAck++;
					sent++;
					acks++;
				}

				// Conexão finalizada, com ou sem sucesso.
				if (response->isACK() && response->isFIN()) {
					Logger::log("Peer ended connection with success at receiving message.", LogLevel::DEBUG);
					close(transientSocketFd.first);
					return true;
				}
				if (response->isFIN()) {
					Logger::log("Peer ended connection.", LogLevel::DEBUG);
					close(transientSocketFd.first);
					return false;
				}

				// Batch acordado, procede para o proximo batch
				if (batchAck == batchSize) {
					Logger::log("Batch " + std::to_string(batchStart + 1) + " aknowledged.", LogLevel::DEBUG);
					break;
				}
			}
		}
		// No final de uma tentativa, verifica se todo o batch foi acordado. Caso não, encerra o fluxo de envio.
		// Caso tenha finalizado de acordar todos os datagramas, finaliza o fluxo.
		if (batchAck != batchSize || acks == totalDatagrams) {
			break;
		}
	}
	close(transientSocketFd.first);
	return acks == totalDatagrams;
}

bool MessageSender::broadcast(std::vector<unsigned char> &message, std::pair<int, sockaddr_in> identifier) {
	auto datagram = Datagram();
	unsigned short totalDatagrams = calculateTotalDatagrams(message.size());
	datagram.setDatagramTotal(totalDatagrams);
	datagram.setSourceAddress(configAddr.sin_addr.s_addr);
	datagram.setSourcePort(configAddr.sin_port);
	datagram.setDestinationPort(identifier.second.sin_port);
	datagram.setDestinAddress(configAddr.sin_addr.s_addr);

	datagramController->createQueue({configAddr.sin_addr.s_addr, identifier.second.sin_port});

	sockaddr_in destin = Protocol::broadcastAddress();
	auto members = std::map<std::pair<unsigned int, unsigned short>, bool>();
	// build of datagrams
	auto datagrams = std::vector<std::vector<unsigned char>>(totalDatagrams);
	std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> membersAcks;
	buildBroadcastDatagrams(&datagrams, &membersAcks, identifier.second.sin_port, totalDatagrams, message, &members);

	if (!broadcastAckAttempts(destin, &datagram, &members)) {
		Logger::log("FAILED IN ACK", LogLevel::INFO);
		close(identifier.first);
		return false;
	}

	unsigned short batchSize = BATCH_SIZE;
	const double batchCount = static_cast<int>(ceil(static_cast<double>(totalDatagrams) / batchSize));
	auto buff = std::vector<unsigned char>(2048);

	for (unsigned short batchStart = 0; batchStart < batchCount; batchStart++) {
		unsigned short batchIndex;
		if (verifyMessageAckedURB(&members)) {
			close(identifier.first);
			return true;
		}
		for (int attempt = 0; attempt < RETRY_DATA_ATTEMPT; attempt++) {
			Logger::log("Retry", LogLevel::DEBUG);
			if (verifyBatchAcked(&membersAcks, batchSize, batchStart, totalDatagrams)) {
				break;
			}
			if (verifyMessageAckedURB(&members)) {
				close(identifier.first);
				return true;
			}

			for (unsigned short j = 0; j < batchSize; j++) {
				batchIndex = batchStart * batchSize + j;
				if (batchIndex >= totalDatagrams)
					break;
				bool datagramAcked = true;
				for (const auto &configPair : *configMap) {
					const auto &config = configPair.second;
					if (!membersAcks[{config.sin_addr.s_addr, config.sin_port}][batchIndex].second) {
						datagramAcked = false;
						break;
					}
				}

				if (!datagramAcked || batchStart == batchCount - 1)
					sendto(broadcastFD, datagrams[batchIndex].data(), datagrams[batchIndex].size(), 0,
						   reinterpret_cast<sockaddr *>(&destin), sizeof(destin));
			}
			while (true) {
				// Batch acordado, procede para o proximo batch
				if (verifyBatchAcked(&membersAcks, batchSize, batchStart, totalDatagrams) &&
					batchStart != batchCount - 1) {
					break;
				}
				// Todos responderam FINACK
				if (verifyMessageAckedURB(&members)) {
					close(identifier.first);
					return true;
				}
				Datagram *response = datagramController->getDatagramTimeout(
					{datagram.getSourceAddress(), datagram.getDestinationPort()}, RETRY_ACK_TIMEOUT_USEC);
				if (response == nullptr)
					break;
				auto identifier = std::make_pair(response->getSourceAddress(), response->getSourcePort());
				// Resposta de outro batch, pode ser descartada.
				if (response->getVersion() < batchStart * batchSize ||
					response->getVersion() > (batchStart * batchSize) + batchSize) {
					Logger::log("Received old response.", LogLevel::DEBUG);
					continue;
				}
				// Conexão finalizada, com ou sem sucesso.
				if (response->isACK() && response->isFIN()) {
					if (members.contains(identifier))
						members[identifier] = true;
				}
				// Armazena informação de ACK recebido.
				if (response->getVersion() <= totalDatagrams && response->isACK() && membersAcks.contains(identifier) &&
					!membersAcks[identifier][response->getVersion() - 1].first) {
					Logger::log("Datagram of version " + std::to_string(response->getVersion()) + " accepted.",
								LogLevel::DEBUG);
					membersAcks[identifier][response->getVersion() - 1] = {true, true};
				}
			}
		}
		// No final das tentativas, verifica se o batch foi acordado. Caso menos de 2f+1 processos tenham acordado o
		// batch, encerra o fluxo de envio. Caso tenha finalizado de acordar todos os datagramas, finaliza o fluxo.
		if (!verifyBatchAckedFaulty(&membersAcks, batchSize, batchStart, totalDatagrams) ||
			verifyMessageAckedURB(&members)) {
			break;
		}
	}
	close(identifier.first);
	return broadcastType == BEB ? verifyMessageAckedBEB(&members) : verifyMessageAckedFaultyURB(&members);
}


void MessageSender::buildSynchronizeDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
								   std::map<unsigned short, bool> *acknowledgments,
								   unsigned short totalDatagrams, std::vector<unsigned char> &message,
								   std::pair<unsigned, unsigned short> origin, std::pair<unsigned, unsigned short> identifier) {
	for (int i = 0; i < totalDatagrams; ++i) {
		auto versionDatagram = Datagram();
		versionDatagram.setSourceAddress(origin.first);
		versionDatagram.setSourcePort(origin.second);
		versionDatagram.setDestinationPort(identifier.second);
		versionDatagram.setDestinAddress(identifier.first);
		versionDatagram.setVersion(i + 1);
		versionDatagram.setDatagramTotal(totalDatagrams);
		for (unsigned short j = 0; j < 2024; j++) {
			const unsigned int index = i * 2024 + j;
			if (index >= message.size())
				break;
			versionDatagram.getData()->push_back(message.at(index));
		}
		versionDatagram.setDataLength(versionDatagram.getData()->size());
		auto serializedDatagram = Protocol::serialize(&versionDatagram);
		Protocol::setChecksum(&serializedDatagram);
		(*datagrams)[i] = serializedDatagram;
		(*acknowledgments)[i] = false;
	}
}

bool MessageSender::synchronizeBroadcast(std::vector<unsigned char> &message,
										 std::pair<unsigned, unsigned short> identifier,
										 std::pair<unsigned, unsigned short> target,
										 std::pair<unsigned, unsigned short> origin) {
	auto datagram = Datagram();
	unsigned short totalDatagrams = calculateTotalDatagrams(message.size());
	datagram.setDatagramTotal(totalDatagrams);
	datagram.setSourceAddress(origin.first);
	datagram.setSourcePort(origin.second);
	datagram.setDestinationPort(identifier.second);
	datagram.setDestinAddress(identifier.first);
	datagramController->createQueue({identifier.first, identifier.second});
	auto destin = Protocol::broadcastAddress();
	bool accepted = synchronizeAckAttempts(destin, &datagram, target);
	if (!accepted) {
		return false;
	}
	// build of datagrams
	std::vector<std::vector<unsigned char>> datagrams = std::vector<std::vector<unsigned char>>(totalDatagrams);
	std::map<unsigned short, bool> acknowledgments;
	buildSynchronizeDatagrams(&datagrams, &acknowledgments, totalDatagrams, message, origin, identifier);

	unsigned short batchSize = BATCH_SIZE, sent = 0, acks = 0;
	const double batchCount = static_cast<int>(ceil(static_cast<double>(totalDatagrams) / batchSize));
	std::vector<unsigned char> buff = std::vector<unsigned char>(2048);
	for (unsigned short batchStart = 0; batchStart < batchCount; batchStart++) {
		unsigned short batchIndex, batchAck = 0;
		if (sent == totalDatagrams) {
			return true;
		}
		for (int attempt = 0; attempt < RETRY_DATA_ATTEMPT; attempt++) {
			if (batchAck == batchSize || acks == totalDatagrams)
				break;

			for (unsigned short j = 0; j < batchSize; j++) {
				batchIndex = batchStart * batchSize + j;
				if (batchIndex >= totalDatagrams)
					break;
				if (acknowledgments[batchIndex])
					continue;
				sendto(broadcastFD, datagrams[batchIndex].data(), datagrams[batchIndex].size(), 0,
					   reinterpret_cast<sockaddr *>(&destin), sizeof(destin));
			}
			while (true) {
				Datagram *response = datagramController->getDatagramTimeout(
					{datagram.getSourceAddress(), datagram.getDestinationPort()}, 100);

				if (response == nullptr)
					break;
				if (response->getSourceAddress() != target.first || response->getDestinationPort() != target.second)
					continue;
				// Resposta de outro batch, pode ser descartada.
				if (response->getVersion() - 1 < batchStart * batchSize ||
					response->getVersion() - 1 > (batchStart * batchSize) + batchSize) {
					Logger::log("Received old response.", LogLevel::DEBUG);
					continue;
				}
				// Armazena informação de ACK recebido.
				if (response->getVersion() - 1 <= totalDatagrams && response->isACK() &&
					!acknowledgments[response->getVersion() - 1]) {
					Logger::log("Datagram of version " + std::to_string(response->getVersion()) + " accepted.",
								LogLevel::DEBUG);
					acknowledgments[response->getVersion() - 1] = true;
					batchAck++;
					sent++;
					acks++;
				}

				// Conexão finalizada, com ou sem sucesso.
				if (response->isACK() && response->isFIN()) {
					Logger::log("Peer ended connection with success at receiving message.", LogLevel::DEBUG);
					return true;
				}
				if (response->isFIN()) {
					Logger::log("Peer ended connection.", LogLevel::DEBUG);
					return false;
				}

				// Batch acordado, procede para o proximo batch
				if (batchAck == batchSize) {
					Logger::log("Batch " + std::to_string(batchStart + 1) + " aknowledged.", LogLevel::DEBUG);
					break;
				}
			}
		}
		// No final de uma tentativa, verifica se todo o batch foi acordado. Caso não, encerra o fluxo de envio.
		// Caso tenha finalizado de acordar todos os datagramas, finaliza o fluxo.
		if (batchAck != batchSize || acks == totalDatagrams) {
			break;
		}
	}
	return acks == totalDatagrams;
}

bool MessageSender::sendBroadcast(std::vector<unsigned char> &message) {
	std::pair<int, sockaddr_in> transientSocketFd = createUDPSocketAndGetPort();
	return broadcast(message, transientSocketFd);
}

void MessageSender::removeFailed(
	std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
	std::map<std::pair<unsigned int, unsigned short>, bool> *members) {
	auto remove = std::vector<std::pair<unsigned int, unsigned short>>();
	for (auto &[addr, map] : *membersAcks) {
		for (auto &&[ack, response] : map) {
			if (!response.first)
				remove.emplace_back(addr.first, addr.second);
		}
	}
	for (auto member : remove) {
		if (membersAcks->contains({member.first, member.second}))
			membersAcks->erase({member.first, member.second});
		if (members->contains({member.first, member.second}))
			members->erase({member.first, member.second});
	}
}
std::pair<int, sockaddr_in> MessageSender::createUDPSocketAndGetPort() {
	sockaddr_in addr{};
	socklen_t addr_len = sizeof(addr);

	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd < 0) {
		Logger::log("Failed to create socket.", LogLevel::ERROR);
		throw std::runtime_error("Failed to create socket");
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(0);

	if (bind(sockfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
		Logger::log("Failed binding socket.", LogLevel::ERROR);
		close(sockfd);
		throw std::runtime_error("Failed to bind socket");
	}

	if (getsockname(sockfd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) < 0) {
		Logger::log("Failed collecting socket information.", LogLevel::ERROR);
		close(sockfd);
		throw std::runtime_error("Failed to get socket name.");
	}

	return {sockfd, addr};
}

bool MessageSender::verifyMessageAckedURB(std::map<std::pair<unsigned int, unsigned short>, bool> *membersAcks) {
	for (auto &&member : *membersAcks) {
		if (!member.second)
			return false;
	}
	return true;
}

bool MessageSender::verifyMessageAckedFaultyURB(std::map<std::pair<unsigned int, unsigned short>, bool> *membersAcks) {
	unsigned long acked = 0;
	for (auto &&member : *membersAcks) {
		if (member.second)
			acked++;
	}
	const auto f = membersAcks->size() - acked;
	return acked >= 2 * f + 1;
}

bool MessageSender::verifyMessageAckedBEB(std::map<std::pair<unsigned int, unsigned short>, bool> *membersAcks) {
	unsigned short totalSYNACK = 0;
	for (auto &&member : *membersAcks) {
		if (member.second)
			totalSYNACK++;
	}
	return totalSYNACK >= membersAcks->size() / 2;
}

bool MessageSender::verifyBatchAcked(
	std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
	unsigned short batchSize, unsigned short batchIndex, unsigned short totalDatagrams) {

	unsigned short batchStart = batchSize * batchIndex, batchEnd = (batchSize * batchIndex) + batchSize;
	for (int i = batchStart; i < batchEnd; ++i) {
		if (i >= totalDatagrams) {
			break;
		}
		for (auto &&member : *membersAcks) {
			for (auto &&[ack, response] : member.second) {
				if (!response.first)
					return false;
			}
		}
	}
	return true;
}

bool MessageSender::verifyBatchAckedFaulty(
	std::map<std::pair<unsigned int, unsigned short>, std::map<unsigned short, std::pair<bool, bool>>> *membersAcks,
	unsigned short batchSize, unsigned short batchIndex, unsigned short totalDatagrams) {
	unsigned long batchAckedMembers = 0;
	unsigned short batchStart = batchSize * batchIndex, batchEnd = (batchSize * batchIndex) + batchSize;
	for (int i = batchStart; i < batchEnd; ++i) {
		if (i >= totalDatagrams) {
			break;
		}
		for (auto &&member : *membersAcks) {
			bool acked = true;
			for (auto &&[ack, response] : member.second) {
				if (!response.first) {
					acked = false;
					break;
				}
			}
			if (acked)
				batchAckedMembers++;
		}
	}
	auto f = membersAcks->size() - batchAckedMembers;
	return batchAckedMembers >= 2 * f + 1;
}

unsigned short MessageSender::calculateTotalDatagrams(unsigned int dataLength) {
	const double result = static_cast<double>(dataLength) / 2024;
	return static_cast<int>(ceil(result));
}

bool MessageSender::synchronizeAckAttempts(sockaddr_in &destin, Datagram *datagram,
										   std::pair<unsigned, unsigned short> target) {
	Flags flags;
	flags.SYN = true;
	Protocol::setFlags(datagram, &flags);
	auto buff = std::vector<unsigned char>(2048);

	for (int i = 0; i < RETRY_ACK_ATTEMPT; ++i) {
		bool sent = Protocol::sendDatagram(datagram, &destin, broadcastFD, &flags);
		if (!sent) {
			Logger::log("Failed sending ACK datagram.", LogLevel::WARNING);
			continue;
		}
		Datagram *response = datagramController->getDatagramTimeout(
			{datagram->getSourceAddress(), datagram->getDestinationPort()}, RETRY_ACK_TIMEOUT_USEC);

		if (response == nullptr) {
			Logger::log("ACK Timedout.", LogLevel::WARNING);
			continue;
		}
		if (response->getSourceAddress() != target.first || response->getSourcePort() != target.second)
			continue;
		if (response->isACK())
			return true;
	}
	Logger::log("ACK failed.", LogLevel::WARNING);

	return false;
}

bool MessageSender::ackAttempts(sockaddr_in &destin, Datagram *datagram) {
	Flags flags;
	flags.SYN = true;
	Protocol::setFlags(datagram, &flags);
	auto buff = std::vector<unsigned char>(2048);

	for (int i = 0; i < RETRY_ACK_ATTEMPT; ++i) {
		bool sent = Protocol::sendDatagram(datagram, &destin, socketFD, &flags);
		if (!sent) {
			Logger::log("Failed sending ACK datagram.", LogLevel::WARNING);
			continue;
		}
		Datagram *response =
			datagramController->getDatagramTimeout({datagram->getSourceAddress(), datagram->getDestinationPort()},
												   RETRY_ACK_TIMEOUT_USEC + RETRY_ACK_TIMEOUT_USEC * i);

		if (response == nullptr) {
			Logger::log("ACK Timedout.", LogLevel::WARNING);
			continue;
		}
		if (response->isACK() && response->isSYN() && datagram->getVersion() == response->getVersion() &&
			response->getSourceAddress() == destin.sin_addr.s_addr &&
			response->getDestinationPort() == destin.sin_port) {
			return true;
		}
	}
	Logger::log("ACK failed.", LogLevel::WARNING);

	return false;
}

bool MessageSender::broadcastAckAttempts(sockaddr_in &destin, Datagram *datagram,
										 std::map<std::pair<unsigned int, unsigned short>, bool> *members) {
	Flags flags;
	flags.SYN = true;
	Protocol::setFlags(datagram, &flags);
	auto buff = std::vector<unsigned char>(2048);
	bool acked = true;
	for (int i = 0; i < RETRY_ACK_ATTEMPT; ++i) {
		for (auto [_, ack] : *members) {
			if (!ack) {
				acked = false;
				break;
			}
		}
		if (acked)
			break;
		bool sent = Protocol::sendDatagram(datagram, &destin, broadcastFD, &flags);
		if (!sent) {
			Logger::log("Failed sending ACK datagram.", LogLevel::WARNING);
			continue;
		}
		while (true) {
			bool acked = true;
			for (auto [_, ack] : *members) {
				if (!ack) {
					acked = false;
					break;
				}
			}
			if (acked)
				break;
			Datagram *response = datagramController->getDatagramTimeout(
				{datagram->getSourceAddress(), datagram->getDestinationPort()}, RETRY_ACK_TIMEOUT_USEC);
			if (response == nullptr) {
				break;
			}
			if (response->isACK() && response->isSYN() && datagram->getVersion() == response->getVersion()) {
				for (auto [_, addr] : *configMap) {
					if (addr.sin_addr.s_addr == datagram->getSourceAddress() &&
						addr.sin_port == datagram->getSourcePort()) {
						(*members)[{response->getSourceAddress(), response->getSourcePort()}] = true;
						break;
					}
				}
			}
		}
	}
	unsigned long totalAcks = 0;
	for (auto [identifier, ack] : *members) {
		if (ack) {
			totalAcks++;
			(*members)[identifier] = false;
		}
	}
	// Diferença entre tamanho do grupo e tamanho de SYN-ACKS
	const unsigned long f = members->size() - totalAcks;
	// Para um processo falho, deve-se ter 3 processos corretos respondendo.
	return totalAcks >= 2 * f + 1;
}
