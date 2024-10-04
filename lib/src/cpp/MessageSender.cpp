#include "../header/MessageSender.h"
#include <cmath>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "../header/ConfigParser.h"
#include "../header/Protocol.h"
#include "../header/BlockingQueue.h"
#include "../header/Logger.h"

#include <cstring>
#include <random>
#include <thread>

#define RETRY_ACK_ATTEMPT 3
#define RETRY_ACK_TIMEOUT_USEC 200

#define RETRY_DATA_ATTEMPT 7
#define RETRY_DATA_TIMEOUT_USEC 100
#define RETRY_DATA_TIMEOUT_USEC_MAX 800
#define TIMEOUT_INCREMENT 200
#define BATCH_SIZE 30

MessageSender::MessageSender(int socketFD)
{
	this->socketFD = socketFD;
}

void MessageSender::buildDatagrams(std::vector<std::vector<unsigned char>> *datagrams,
                                   std::map<unsigned short, bool> *acknowledgments,
                                   std::map<unsigned short, bool> *responses, in_port_t transientPort,
                                   unsigned short totalDatagrams, std::vector<unsigned char> &message)
{
	for (int i = 0; i < totalDatagrams; ++i)
	{
		auto versionDatagram = Datagram();
		versionDatagram.setSourcePort(transientPort);
		versionDatagram.setVersion(i + 1);
		versionDatagram.setDatagramTotal(totalDatagrams);
		for (unsigned short j = 0; j < 1024; j++)
		{
			const unsigned int index = i * 1024 + j;
			if (index >= message.size())
				break;
			versionDatagram.getData()->push_back(message.at(index));
		}
		versionDatagram.setDataLength(versionDatagram.getData()->size());
		auto serializedDatagram = Protocol::serialize(&versionDatagram);
		Protocol::setChecksum(&serializedDatagram);
		(*datagrams)[i] = serializedDatagram;
		(*acknowledgments)[i] = false;
		(*responses)[i] = false;
	}
}

bool MessageSender::sendMessage(sockaddr_in &destin, std::vector<unsigned char> &message)
{
	std::pair<int, sockaddr_in> transientSocketFd = createUDPSocketAndGetPort();
	auto datagram = Datagram();
	unsigned short totalDatagrams = calculateTotalDatagrams(message.size());
	datagram.setDatagramTotal(totalDatagrams);
	datagram.setSourcePort(transientSocketFd.second.sin_port);
	bool accepted = ackAttempts(transientSocketFd.first, destin, &datagram);
	if (!accepted)
	{
		return false;
	}
	// build of datagrams
	std::vector<std::vector<unsigned char>> datagrams = std::vector<std::vector<unsigned char>>(totalDatagrams);
	std::map<unsigned short, bool> acknowledgments, responses;
	buildDatagrams(&datagrams, &acknowledgments, &responses, transientSocketFd.second.sin_port, totalDatagrams,
	               message);

	unsigned short batchSize = BATCH_SIZE, sent = 0, acks = 0;
	const double batchCount = static_cast<int>(ceil(static_cast<double>(totalDatagrams) / batchSize));
	std::vector<unsigned char> buff = std::vector<unsigned char>(1040);
	for (unsigned short batchStart = 0; batchStart < batchCount; batchStart++)
	{
		Datagram response;

		unsigned short batchIndex, batchAck = 0;
		if (sent == totalDatagrams)
		{
			close(transientSocketFd.first);
			return true;
		}
		for (int attempt = 0; attempt < RETRY_DATA_ATTEMPT; attempt++)
		{
			if (batchAck == batchSize || acks == totalDatagrams)
				break;

			for (unsigned short j = 0; j < batchSize; j++)
			{
				batchIndex = batchStart * batchSize + j;
				if (batchIndex >= totalDatagrams)
					break;
				if (acknowledgments[batchIndex])
					continue;
				sendto(socketFD, datagrams[batchIndex].data(),
				       datagrams[batchIndex].size(),
				       0,
				       reinterpret_cast<sockaddr *>(&destin), sizeof(destin));

			}
			while (Protocol::readDatagramSocketTimeout(&response, transientSocketFd.first, &destin,
			                                           RETRY_ACK_TIMEOUT_USEC + RETRY_ACK_TIMEOUT_USEC * attempt,
			                                           &buff))
			{
				// Resposta de outro batch, pode ser descartada.
				if (response.getVersion() - 1 < batchStart * batchSize || response.getVersion() - 1 > (batchStart *
					batchSize) + batchSize)
				{
					Logger::log("Received old response.", LogLevel::DEBUG);
					continue;
				}
				// Armazena informação de ACK recebido.
				if (response.getVersion() - 1 <= totalDatagrams && response.isACK() && !acknowledgments[response.
					getVersion() - 1])
				{
					Logger::log("Datagram of version " + std::to_string(response.getVersion()) + " accepted.",
					            LogLevel::DEBUG);
					acknowledgments[response.getVersion() - 1] = true;
					responses[response.getVersion() - 1] = true;
					batchAck++;
					sent++;
					acks++;
				}

				// Conexão finalizada, com ou sem sucesso.
				if (response.isACK() && response.isFIN())
				{
					Logger::log("Peer ended connection with success at receiving message.", LogLevel::DEBUG);
					close(transientSocketFd.first);
					return true;
				}
				if (response.isFIN())
				{
					Logger::log("Peer ended connection.", LogLevel::DEBUG);
					close(transientSocketFd.first);
					return false;
				}

				// Informa que a versão foi negada.
				if (response.isNACK())
				{
					responses[response.getVersion() - 1] = true;
				}

				// Batch acordado, procede para o proximo batch
				if (batchAck == batchSize)
				{
					Logger::log("Batch " + std::to_string(batchStart + 1) + " aknowledged.", LogLevel::DEBUG);
					break;
				}

				// Verifica se o batch foi ao menos respondido, caso tenha sido, mesmo que com algum NACK, procede para
				// retransmissão (se for o caso).
				auto responsesCounter = 0;
				for (auto &&p : responses)
					if (p.second)
						++responsesCounter;
				if (responsesCounter == batchSize)
				{
					Logger::log("Batch " + std::to_string(batchStart + 1) + " responded, but not aknowledged.",
					            LogLevel::DEBUG);
					break;
				}
			}
		}
		// No final de uma tentativa, verifica se todo o batch foi acordado. Caso não, encerra o fluxo de envio.
		// Caso tenha finalizado de acordar todos os datagramas, finaliza o fluxo.
		if (batchAck != batchSize || acks == totalDatagrams)
		{
			break;
		}
	}
	close(transientSocketFd.first);
	return acks == totalDatagrams;
}

bool MessageSender::sendBroadcast(std::vector<unsigned char> &message)
{
	std::pair<int, sockaddr_in> transientSocketFd = createUDPSocketAndGetPort();

	constexpr int broadcastPermission = 1;
	if (setsockopt(transientSocketFd.first, SOL_SOCKET, SO_BROADCAST, &broadcastPermission, sizeof(broadcastPermission)) < 0) {
		Logger::log("Failed to set broadcast permission.", LogLevel::ERROR);
		close(transientSocketFd.first);
		return true;
	}

	auto datagram = Datagram();
	unsigned short totalDatagrams = calculateTotalDatagrams(message.size());
	datagram.setDatagramTotal(totalDatagrams);
	datagram.setSourcePort(transientSocketFd.second.sin_port);
	// build of datagrams
	auto datagrams = std::vector<std::vector<unsigned char>>(totalDatagrams);
	std::map<unsigned short, bool> acknowledgments, responses;
	buildDatagrams(&datagrams, &acknowledgments, &responses, transientSocketFd.second.sin_port, totalDatagrams,
				   message);

	auto buff = std::vector<unsigned char>(1040);

	sockaddr_in destin = broadcastAddress();

	if (sendto(transientSocketFd.first, datagrams[0].data(), datagrams[0].size(), 0, reinterpret_cast<sockaddr *>(&destin), sizeof(destin)) < 0) {
		perror("Send failed");
		close(transientSocketFd.first);
		return true;
	}

	if (Protocol::readDatagramSocketTimeout(&datagram, transientSocketFd.first, &destin,
													   0,
													   &buff))

	close(transientSocketFd.first);
	return true;
}

sockaddr_in MessageSender::broadcastAddress()
{
	sockaddr_in broadcastAddr{};
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_port = htons(8080); // Should be sent to all ports
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
	return broadcastAddr;
}

std::pair<int, sockaddr_in> MessageSender::createUDPSocketAndGetPort()
{
	sockaddr_in addr{};
	socklen_t addr_len = sizeof(addr);

	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd < 0)
	{
		Logger::log("Failed to create socket.", LogLevel::ERROR);
		throw std::runtime_error("Failed to create socket");
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(0);

	if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		Logger::log("Failed binding socket.", LogLevel::ERROR);
		close(sockfd);
		throw std::runtime_error("Failed to bind socket");
	}

	if (getsockname(sockfd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) < 0)
	{
		Logger::log("Failed collecting socket information.", LogLevel::ERROR);
		close(sockfd);
		throw std::runtime_error("Failed to get socket name.");
	}

	return {sockfd, addr};
}

unsigned short MessageSender::calculateTotalDatagrams(unsigned int dataLength)
{
	const double result = static_cast<double>(dataLength) / 1024;
	return static_cast<int>(ceil(result));
}

bool MessageSender::ackAttempts(int transientSocketfd, sockaddr_in &destin, Datagram *datagram)
{
	Flags flags;
	flags.SYN = true;
	Datagram response;
	Protocol::setFlags(datagram, &flags);
	auto buff = std::vector<unsigned char>(1040);

	for (int i = 0; i < RETRY_ACK_ATTEMPT; ++i)
	{
		bool sent = Protocol::sendDatagram(datagram, &destin, socketFD, &flags);
		if (!sent)
		{
			Logger::log("Failed sending ACK datagram.", LogLevel::INFO);
			continue;
		}
		sockaddr_in senderAddr{};
		senderAddr.sin_family = AF_INET;
		sent = Protocol::readDatagramSocketTimeout(&response, transientSocketfd, &senderAddr,
		                                           RETRY_ACK_TIMEOUT_USEC + RETRY_ACK_TIMEOUT_USEC * i, &buff);
		if (!sent)
			continue;
		if (response.isACK() && response.isSYN() && datagram->getVersion() == response.getVersion())
		{
			return true;
		}
	}
	Logger::log("ACK failed.", LogLevel::INFO);

	return false;
}
