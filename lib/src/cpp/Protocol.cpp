#include "../header/Protocol.h"

#include <Logger.h>
#include <csignal>

#include <fcntl.h>
#include <future>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <vector>
#include "TypeUtils.h"

#include "../header/Flags.h"
// Serializes data and computes the checksum while doing so.
std::vector<unsigned char> Protocol::serialize(Datagram *datagram) {
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
	(*data)[20] = static_cast<unsigned char>(checksum & 0xFF);
	(*data)[21] = static_cast<unsigned char>((checksum >> 8) & 0xFF);
	(*data)[22] = static_cast<unsigned char>((checksum >> 16) & 0xFF);
	(*data)[23] = static_cast<unsigned char>((checksum >> 24) & 0xFF);
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

// Computes the checksum of a datagram, the checksum field will be zero while computing.
unsigned int Protocol::computeChecksum(std::vector<unsigned char> *serializedDatagram) {
	(*serializedDatagram)[20] = 0;
	(*serializedDatagram)[21] = 0;
	(*serializedDatagram)[22] = 0;
	(*serializedDatagram)[23] = 0;
	return sumChecksum32(serializedDatagram);
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

bool Protocol::verifyChecksum(Datagram *datagram, std::vector<unsigned char> *serializedDatagram) {
	auto dch = datagram->getChecksum();
	auto cch = computeChecksum(serializedDatagram);
	return dch == cch;
}

thread_local std::atomic<std::thread::id> Protocol::timeoutThreadId = std::this_thread::get_id();
thread_local std::atomic<bool> Protocol::waitingTimeout = false;

void Protocol::signalHandler(int) {
	std::thread::id currentThreadId = std::this_thread::get_id();
	if (currentThreadId == timeoutThreadId.load() && waitingTimeout.load()) {
		waitingTimeout.store(false);
		throw std::exception();
	}
}

bool Protocol::readDatagramSocketTimeout(Datagram *datagramBuff, int socketfd, sockaddr_in *senderAddr, int timeoutMS,
										 std::vector<unsigned char> *buff) {
	std::signal(SIGALRM, signalHandler);

	sigset_t newmask, oldmask;
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &newmask, &oldmask);
	ualarm(timeoutMS * 1000, 0);
	waitingTimeout.store(true);
	try {
		pthread_sigmask(SIG_UNBLOCK, &newmask, nullptr);
		readDatagramSocket(datagramBuff, socketfd, senderAddr, buff);
		ualarm(0, 0);
		pthread_sigmask(SIG_SETMASK, &oldmask, nullptr);
		return true;
	}
	catch (const std::exception &) {
		ualarm(0, 0);
		pthread_sigmask(SIG_SETMASK, &oldmask, nullptr);
		return false;
	}
}

bool Protocol::readDatagramSocket(Datagram *datagramBuff, int socketfd, sockaddr_in *senderAddr,
								   std::vector<unsigned char> *buff) {
	socklen_t senderAddrLen = sizeof(sockaddr_in);
	ssize_t bytes_received = recvfrom(socketfd, buff->data(), buff->size(), 0,
	                                  reinterpret_cast<struct sockaddr *>(senderAddr), &senderAddrLen);
	if (bytes_received < 0)
		return false;
	bufferToDatagram(*datagramBuff, *buff);
	const std::vector dataVec(buff->begin() + 24, buff->begin() + 24 + datagramBuff->getDataLength());
	datagramBuff->setData(dataVec);
	return true;
}

void Protocol::bufferToDatagram(Datagram &datagramBuff, const std::vector<unsigned char> &bytesBuffer) {
	datagramBuff.setSourceAddress(TypeUtils::buffToUnsignedInt(bytesBuffer, 0));
	datagramBuff.setSourceAddress(TypeUtils::buffToUnsignedInt(bytesBuffer, 4));
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
	if (flags->NACK)
		datagram->setIsNACK();
	if (flags->SYN)
		datagram->setIsSYN();
	if (flags->FIN)
		datagram->setIsFIN();
	if (flags->END)
		datagram->setIsEND();
}
