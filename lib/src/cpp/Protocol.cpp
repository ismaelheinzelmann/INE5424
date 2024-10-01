#include "../header/Protocol.h"

#include <Logger.h>
#include <csignal>

#include "TypeUtils.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <random>
#include <vector>
#include <future>
#include <fcntl.h>

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

volatile sig_atomic_t Protocol::timeOut = false;

void Protocol::setChecksum(std::vector<unsigned char> *data)
{
	auto checksum = sumChecksum32(data);
	(*data)[15] = static_cast<unsigned char>(checksum & 0xFF);
	(*data)[14] = static_cast<unsigned char>((checksum >> 8) & 0xFF);
	(*data)[13] = static_cast<unsigned char>((checksum >> 16) & 0xFF);
	(*data)[12] = static_cast<unsigned char>((checksum >> 24) & 0xFF);
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
	(*serializedDatagram)[12] = 0;
	(*serializedDatagram)[13] = 0;
	(*serializedDatagram)[14] = 0;
	(*serializedDatagram)[15] = 0;
	return sumChecksum32(serializedDatagram);
}

unsigned int Protocol::sumChecksum32(const std::vector<unsigned char> *data)
{
	unsigned int crc = 0xFFFFFFFF; // start
	unsigned int polynomial = 0xEDB88320; // The polynomial for CRC-32 standard

	// for each byte
	for (unsigned char byte : *data)
	{
		crc ^= byte; // XOR byte with the current remainder
		for (int i = 0; i < 8; ++i)
		{
			// for each byte
			if (crc & 1)
			{
				crc = (crc >> 1) ^ polynomial;
			}
			else
			{
				crc >>= 1;
			}
		}
	}

	// invert the bits to get the final CRC
	return crc ^ 0xFFFFFFFF;
}

bool Protocol::verifyChecksum(Datagram *datagram, std::vector<unsigned char> *serializedDatagram)
{
	auto dch = datagram->getChecksum();
	auto cch = computeChecksum(serializedDatagram);
	return dch == cch;
}

void Protocol::handleTimeout(int sig)
{
	Logger::log(std::to_string(sig), LogLevel::NONE);
	timeOut = 0;
}


bool Protocol::readDatagramSocketTimeout(Datagram *datagramBuff,
                                         int socketfd,
                                         sockaddr_in *senderAddr,
                                         int timeoutMS, std::vector<unsigned char> *buff)
{
	int flags = fcntl(socketfd, F_GETFL, 0);
	fcntl(socketfd, F_SETFL, flags | O_NONBLOCK);
	auto start_time = std::chrono::steady_clock::now();
	socklen_t senderAddrLen = sizeof(*senderAddr);
	while (true)
	{
		ssize_t bytes_received = recvfrom(socketfd, buff->data(), buff->size(), 0,
		                                  reinterpret_cast<sockaddr *>(senderAddr), &senderAddrLen);
		if (bytes_received > 0)
		{
			bufferToDatagram(*datagramBuff, *buff);
			flags = fcntl(socketfd, F_GETFL, 0);
			flags &= ~O_NONBLOCK;
			fcntl(socketfd, F_SETFL, flags);
			return true;
		}

		auto now = std::chrono::steady_clock::now();
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

		if (elapsed_ms > timeoutMS)
		{
			Logger::log("Timed out with time " + std::to_string(elapsed_ms), LogLevel::WARNING);
			flags = fcntl(socketfd, F_GETFL, 0);
			flags &= ~O_NONBLOCK;
			fcntl(socketfd, F_SETFL, flags);
			return false;
		}
		usleep(10000);
	}
}

bool Protocol::readDatagramSocket(Datagram *datagramBuff, int socketfd, sockaddr_in *senderAddr,
                                  std::vector<unsigned char> *buff)
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

bool Protocol::sendDatagram(Datagram *datagram, sockaddr_in *to, int socketfd, Flags *flags)
{
	// if (randomReturnPercent()) return true;
	setFlags(datagram, flags);
	auto serializedDatagram = serialize(datagram);
	setChecksum(&serializedDatagram);
	const ssize_t bytes = sendto(socketfd, serializedDatagram.data(), serializedDatagram.size(), 0,
	                             reinterpret_cast<struct sockaddr *>(to), sizeof(*to));
	return bytes == static_cast<ssize_t>(serializedDatagram.size());
}

void Protocol::setFlags(Datagram *datagram, Flags *flags)
{
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

