
#include "../header/TypeUtils.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

unsigned short TypeUtils::buffToUnsignedShort(const std::vector<unsigned char> &buffer, unsigned int i) {
	// Ensure the buffer has enough data
	if (i + 1 >= buffer.size()) {
		throw std::out_of_range("Buffer overflow: not enough data to read an unsigned short.");
	}
	unsigned char buff[2] = {buffer.at(i), buffer.at(i + 1)};

	// Read two bytes from the buffer
	return (buff[0] << 8) | buff[1];
}

unsigned int TypeUtils::buffToUnsignedInt(const std::vector<unsigned char> &buffer, unsigned int i) {
	// Ensure the buffer has enough data
	if (i + 3 >= buffer.size()) {
		throw std::out_of_range("Buffer overflow: not enough data to read an unsigned int.");
	}

	// Read 4 bytes from the buffer in big-endian order
	const unsigned int value = (static_cast<unsigned int>(buffer[i]) << 24) |
		(static_cast<unsigned int>(buffer[i + 1]) << 16) | (static_cast<unsigned int>(buffer[i + 2]) << 8) |
		(static_cast<unsigned int>(buffer[i + 3]));

	return value;
}


void TypeUtils::uintToBytes(unsigned int value, unsigned char bytes[4]) {
	bytes[0] = static_cast<unsigned char>(value & 0xFF); // Extract least significant byte
	bytes[1] = static_cast<unsigned char>((value >> 8) & 0xFF); // Extract second byte
	bytes[2] = static_cast<unsigned char>((value >> 16) & 0xFF); // Extract third byte
	bytes[3] = static_cast<unsigned char>((value >> 24) & 0xFF); // Extract most significant byte
}
