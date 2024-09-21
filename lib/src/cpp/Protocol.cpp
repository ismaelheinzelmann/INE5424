#include "../header/Protocol.h"
#include "../header/CRC32.h"
#include "TypeUtils.h"
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/select.h>
#include <cstdio>
#include <fcntl.h>
#include <random>

#include "../header/Flags.h"
// Serializes data and computes the checksum while doing so.
std::vector<unsigned char> Protocol::serialize(Datagram *datagram)
{
	// TODO Refatorar
	std::vector<unsigned char> serializedDatagram;

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

void Protocol::setChecksum(std::vector<unsigned char> *data)
{
	auto checksum = computeChecksum(data);
	memcpy(data->data() + 12, &checksum, sizeof(checksum));
}

// Deserializes data and returns a Datagram object.
Datagram Protocol::deserialize(std::vector<unsigned char> &serializedDatagram)
{
	Datagram datagram;
	bufferToDatagram(datagram, serializedDatagram);
	std::vector<unsigned char> data;
	for (unsigned int i = 0; i < datagram.getDataLength(); i++)
	{
		data.push_back(serializedDatagram[i + 16]);
	}
	datagram.setData(data);
	return datagram;
}

// Computes the checksum of a datagram, the checksum field will be zero while computing.
unsigned int Protocol::computeChecksum(std::vector<unsigned char> *serializedDatagram)
{
	unsigned char buff[2] = {0, 0};
	buff[0] = serializedDatagram->at(8);
	buff[1] = serializedDatagram->at(9);
	auto *length = reinterpret_cast<unsigned short *>(buff);
	auto serializedData = std::vector<unsigned char>();
	for (int i = 0; i < 12+ (*length); ++i)
	{
		serializedData.push_back(serializedDatagram->at(i));
	}
	for (int i = 0; i < 4; ++i)
	{
		serializedData.push_back(0);
	}
	for (unsigned int i = 0; i < (*length); i++)
	{
		serializedData.push_back(serializedData[i]+16);
	}
	return CRC32::calculate(serializedData);
}

bool Protocol::verifyChecksum(Datagram *datagram, std::vector<unsigned char> *serializedDatagram)
{
	auto dch = datagram->getChecksum();
	auto cch = computeChecksum(serializedDatagram);
	return dch == cch;
}

bool Protocol::readDatagramSocketTimeout(Datagram &datagramBuff, int socketfd, sockaddr_in &senderAddr, int timeoutMS)
{
	fd_set read_fds;
	timeval timeout{};
	timeout.tv_sec = timeoutMS / 1000;
	timeout.tv_usec = (timeoutMS % 1000) * 1000;

	// Initialize the file descriptor set
	FD_ZERO(&read_fds);
	FD_SET(socketfd, &read_fds);

	// Wait for data to be available on the socket
	int select_result = select(socketfd + 1, &read_fds, nullptr, nullptr, &timeout);

	if (select_result < 0)
	{
		// Error occurred in select()
		perror("select");
		return false;
	}
	if (select_result == 0)
	{
		// Timeout occurred
		std::cerr << "Timeout occurred\n";
		return false;
	}

	// Check if the socket is readable
	if (FD_ISSET(socketfd, &read_fds))
	{
		socklen_t senderAddrLen = sizeof(senderAddr);
		auto bytesBuffer = std::vector<unsigned char>(1040);
		ssize_t bytes_received = recvfrom(socketfd, bytesBuffer.data(), bytesBuffer.size(), 0,
		                                  reinterpret_cast<struct sockaddr *>(&senderAddr), &senderAddrLen);
		if (bytes_received < 0)
		{
			// Error occurred in recvfrom()
			perror("recvfrom");
			return false;
		}

		// Process the received data
		bufferToDatagram(datagramBuff, bytesBuffer);
		// Ensure we don't go out of bounds
		if (bytes_received > 16)
		{
			const std::vector dataVec(
				bytesBuffer.begin() + 16,
				bytesBuffer.begin() + 16 + datagramBuff.getDataLength()
				);
			datagramBuff.setData(dataVec);
		}
		return true;
	}

	// Socket was not ready for reading
	return false;
}

bool Protocol::readDatagramSocket(Datagram *datagramBuff, int socketfd, sockaddr_in *senderAddr, std::vector<unsigned char>* buff)
{
	socklen_t senderAddrLen = sizeof(senderAddr);
	ssize_t bytes_received = recvfrom(socketfd, buff->data(), buff->size(), 0,
	                                  reinterpret_cast<struct sockaddr *>(senderAddr), &senderAddrLen);
	if (bytes_received < 0)
		return false;
	bufferToDatagram(*datagramBuff, *buff);
	const std::vector dataVec(buff->begin() + 16, buff->begin() + 16 + datagramBuff->getDataLength());
	datagramBuff->setData(dataVec);
	return true;
}

void Protocol::bufferToDatagram(Datagram &datagramBuff, const std::vector<unsigned char> &bytesBuffer)
{
	datagramBuff.setSourcePort(TypeUtils::buffToUnsignedShort(bytesBuffer, 0));
	datagramBuff.setDestinationPort(TypeUtils::buffToUnsignedShort(bytesBuffer, 2));
	datagramBuff.setVersion(TypeUtils::buffToUnsignedShort(bytesBuffer, 4));
	datagramBuff.setDatagramTotal(TypeUtils::buffToUnsignedShort(bytesBuffer, 6));
	datagramBuff.setDataLength(TypeUtils::buffToUnsignedShort(bytesBuffer, 8));
	datagramBuff.setFlags(TypeUtils::buffToUnsignedShort(bytesBuffer, 10));
	datagramBuff.setChecksum(TypeUtils::buffToUnsignedInt(bytesBuffer, 12));
}

bool randomReturnPercent() {
	// Create a random number generator
	static std::random_device rd;  // Obtain a random number from hardware
	static std::mt19937 eng(rd());  // Seed the generator
	static std::uniform_int_distribution<> distr(1, 100); // Define the range

	// Generate a random number between 1 and 100
	int randomValue = distr(eng);

	// Return true if the random number is less than or equal to 70
	return randomValue <= 70;
}

bool Protocol::sendDatagram(Datagram *datagram, sockaddr_in *to, int socketfd, Flags *flags)
{
	// if (randomReturnPercent()) return true;
	setFlags(datagram, flags);
	const auto serializedDatagram = serialize(datagram);
	const ssize_t bytes = sendto(socketfd, serializedDatagram.data(), serializedDatagram.size(), 0,
	                             reinterpret_cast<struct sockaddr *>(to), sizeof(*to));
	return bytes == static_cast<ssize_t>(serializedDatagram.size());
}

void Protocol::setFlags(Datagram *datagram, Flags *flags)
{
	if (flags->ACK) datagram->setIsACK();
	if (flags->NACK) datagram->setIsNACK();
	if (flags->SYN) datagram->setIsSYN();
	if (flags->FIN) datagram->setIsFIN();
}

