#include "../header/Protocol.h"

#include <Logger.h>
#include <Request.h>

#include <future>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>
#include "TypeUtils.h"
#include "FaultInjector.h"

#include "../header/Flags.h"
// Serializes data and computes the checksum while doing so.
std::vector<unsigned char> Protocol::serialize(Datagram *datagram) {
	// TODO Refatorar
	std::vector<unsigned char> serializedDatagram;

	unsigned int tempInt = datagram->getSourceAddress();
	serializedDatagram.push_back(static_cast<unsigned char>((tempInt >> 24) & 0xFF));
	serializedDatagram.push_back(static_cast<unsigned char>((tempInt >> 16) & 0xFF));
	serializedDatagram.push_back(static_cast<unsigned char>((tempInt >> 8) & 0xFF));
	serializedDatagram.push_back(static_cast<unsigned char>(tempInt & 0xFF));

	tempInt = datagram->getDestinAddress();
	serializedDatagram.push_back(static_cast<unsigned char>((tempInt >> 24) & 0xFF));
	serializedDatagram.push_back(static_cast<unsigned char>((tempInt >> 16) & 0xFF));
	serializedDatagram.push_back(static_cast<unsigned char>((tempInt >> 8) & 0xFF));
	serializedDatagram.push_back(static_cast<unsigned char>(tempInt & 0xFF));

	unsigned short temp = datagram->getSourcePort();
	serializedDatagram.push_back(static_cast<unsigned char>(temp >> 8));
	serializedDatagram.push_back(static_cast<unsigned char>(temp & 0xFF));

	temp = datagram->getDestinationPort();
	serializedDatagram.push_back(static_cast<unsigned char>(temp >> 8));
	serializedDatagram.push_back(static_cast<unsigned char>(temp & 0xFF));

	temp = datagram->getVersion();
	serializedDatagram.push_back(static_cast<unsigned char>(temp >> 8));
	serializedDatagram.push_back(static_cast<unsigned char>(temp & 0xFF));

	temp = datagram->getDatagramTotal();
	serializedDatagram.push_back(static_cast<unsigned char>(temp >> 8));
	serializedDatagram.push_back(static_cast<unsigned char>(temp & 0xFF));

	temp = datagram->getDataLength();
	serializedDatagram.push_back(static_cast<unsigned char>(temp >> 8));
	serializedDatagram.push_back(static_cast<unsigned char>(temp & 0xFF));

	temp = datagram->getFlags();
	serializedDatagram.push_back(static_cast<unsigned char>(temp >> 8));
	serializedDatagram.push_back(static_cast<unsigned char>(temp & 0xFF));

	for (unsigned short i = 0; i < 4; i++)
		serializedDatagram.push_back(0);
	for (unsigned int i = 0; i < datagram->getDataLength(); i++)
		serializedDatagram.push_back((*datagram->getData())[i]);
	return serializedDatagram;
}

void Protocol::setChecksum(std::vector<unsigned char> *data) {
	auto checksum = sumChecksum32(data);
	(*data)[23] = static_cast<unsigned char>(checksum & 0xFF);
	(*data)[22] = static_cast<unsigned char>((checksum >> 8) & 0xFF);
	(*data)[21] = static_cast<unsigned char>((checksum >> 16) & 0xFF);
	(*data)[20] = static_cast<unsigned char>((checksum >> 24) & 0xFF);
}

// Deserializes data and returns a Datagram object.
Datagram Protocol::deserialize(std::vector<unsigned char> &serializedDatagram) {
	Datagram datagram;
	bufferToDatagram(datagram, serializedDatagram);
	std::vector<unsigned char> data;
	for (unsigned int i = 0; i < datagram.getDataLength(); i++) {
		data.push_back(serializedDatagram[i + 24]);
	}
	datagram.setData(data);
	return datagram;
}

unsigned int Protocol::sumChecksum32(const std::vector<unsigned char> *data) {
	unsigned int crc = 0xFFFFFFFF; // start
	unsigned int polynomial = 0xEDB88320; // The polynomial for CRC-32 standard

	// for each byte
	for (unsigned char byte : *data) {
		crc ^= byte; // XOR byte with the current remainder
		for (int i = 0; i < 8; ++i) {
			// for each byte
			if (crc & 1) {
				crc = (crc >> 1) ^ polynomial;
			}
			else {
				crc >>= 1;
			}
		}
	}

	// invert the bits to get the final CRC
	return crc ^ 0xFFFFFFFF;
}

// Computes the checksum of a datagram, the checksum field will be zero while computing.
unsigned int Protocol::computeChecksum(std::vector<unsigned char> *serializedDatagram) {
	(*serializedDatagram)[20] = 0;
	(*serializedDatagram)[21] = 0;
	(*serializedDatagram)[22] = 0;
	(*serializedDatagram)[23] = 0;
	return sumChecksum32(serializedDatagram);
}

bool Protocol::verifyChecksum(Datagram *datagram, std::vector<unsigned char> *serializedDatagram) {
	auto dch = datagram->getChecksum();
	auto cch = computeChecksum(serializedDatagram);
	return dch == cch;
}

bool Protocol::readDatagramSocket(Datagram *datagramBuff, int socketfd, sockaddr_in *senderAddr,
								  std::vector<unsigned char> *buff, int dropChance, int corruptChance) {
	socklen_t senderAddrLen = sizeof(senderAddr);
	ssize_t bytes_received = recvfrom(socketfd, buff->data(), buff->size(), 0,
									  reinterpret_cast<struct sockaddr *>(senderAddr), &senderAddrLen);
	if (bytes_received < 0)
		return false;
	buff->resize(bytes_received);
	if (generateFault(buff, dropChance, corruptChance))
		return false; // Package dropped

	unsigned int checkSum = TypeUtils::buffToUnsignedInt(*buff, 20);
	unsigned int computedCheckSum = computeChecksum(buff);
	if (checkSum != computedCheckSum) {
		Logger::log("Packet received is now corrupted and will not be responded.", LogLevel::FAULT);
		// Package corrupted
		return false;
	}
	bufferToDatagram(*datagramBuff, *buff);
	const std::vector dataVec(buff->begin() + 24, buff->begin() + 24 + datagramBuff->getDataLength());
	datagramBuff->setData(dataVec);
	return true;
}

// Will return true if the packet should be dropped.
bool Protocol::generateFault(std::vector<unsigned char>* data, int dropChance, int corruptChance) {
	if (FaultInjector::returnTrueByChance(dropChance)) {
		Logger::log("Packet received will be dropped and ignored.", LogLevel::FAULT);
		return true;
	}
	if (FaultInjector::returnTrueByChance(corruptChance)) {
		FaultInjector::corruptVector(data);
	}
	return false;
}

void Protocol::bufferToDatagram(Datagram &datagramBuff, const std::vector<unsigned char> &bytesBuffer) {
	datagramBuff.setSourceAddress(TypeUtils::buffToUnsignedInt(bytesBuffer, 0));
	datagramBuff.setDestinAddress(TypeUtils::buffToUnsignedInt(bytesBuffer, 4));
	datagramBuff.setSourcePort(TypeUtils::buffToUnsignedShort(bytesBuffer, 8));
	datagramBuff.setDestinationPort(TypeUtils::buffToUnsignedShort(bytesBuffer, 10));
	datagramBuff.setVersion(TypeUtils::buffToUnsignedShort(bytesBuffer, 12));
	datagramBuff.setDatagramTotal(TypeUtils::buffToUnsignedShort(bytesBuffer, 14));
	datagramBuff.setDataLength(TypeUtils::buffToUnsignedShort(bytesBuffer, 16));
	datagramBuff.setFlags(TypeUtils::buffToUnsignedShort(bytesBuffer, 18));
	datagramBuff.setChecksum(TypeUtils::buffToUnsignedInt(bytesBuffer, 20));
}

bool Protocol::sendDatagram(Datagram *datagram, sockaddr_in *to, int socketfd, Flags *flags) {
	// if (randomReturnPercent()) return true;
	setFlags(datagram, flags);
	auto serializedDatagram = serialize(datagram);
	setChecksum(&serializedDatagram);
	const ssize_t bytes = sendto(socketfd, serializedDatagram.data(), serializedDatagram.size(), 0,
								 reinterpret_cast<struct sockaddr *>(to), sizeof(*to));
	return bytes == static_cast<ssize_t>(serializedDatagram.size());
}

void Protocol::setFlags(Datagram *datagram, Flags *flags) {
	if (flags->ACK)
		datagram->setIsACK();
	if (flags->SYN)
		datagram->setIsSYN();
	if (flags->FIN)
		datagram->setIsFIN();
	if (flags->END)
		datagram->setIsEND();
	if (flags->BROADCAST)
		datagram->setIsBROADCAST();
	if (flags->HEARTBEAT)
		datagram->setIsHEARTBEAT();
}

void Protocol::setBroadcast(Request *request) {
	auto broadcastAddr = broadcastAddress();
	request->clientRequest->sin_addr.s_addr = broadcastAddr.sin_addr.s_addr;
	request->clientRequest->sin_port = broadcastAddr.sin_port;
	request->clientRequest->sin_family = broadcastAddr.sin_family;
}


sockaddr_in Protocol::broadcastAddress() {
	sockaddr_in broadcastAddr{};
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_port = htons(8888); // Should be sent to all ports
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
	return broadcastAddr;
}
