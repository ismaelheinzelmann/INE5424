
#include "../header/TypeUtils.h"
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/select.h>
#include <cerrno>

unsigned short TypeUtils::buffToUnsignedShort(const std::vector<unsigned char> &buffer, unsigned int i) {
	// Ensure the buffer has enough data
	if (i + 1 >= buffer.size()) {
		throw std::out_of_range("Buffer overflow: not enough data to read an unsigned short.");
	}

	unsigned short value;
	std::memcpy(&value, &buffer[i], sizeof(value));

	// value = (value >> 8) | (value << 8);

	return value;
}

unsigned int TypeUtils::buffToUnsignedInt(const std::vector<unsigned char> &buffer, unsigned int i) {
	// Ensure the buffer has enough data
	if (i + 3 >= buffer.size()) {
		throw std::out_of_range("Buffer overflow: not enough data to read an unsigned int.");
	}

	// Read 4 bytes from the buffer in big-endian order
	const unsigned int value = (static_cast<unsigned int>(buffer[i]) << 24) |
						 (static_cast<unsigned int>(buffer[i + 1]) << 16) |
						 (static_cast<unsigned int>(buffer[i + 2]) << 8)  |
						 (static_cast<unsigned int>(buffer[i + 3]));

	return value;
}


void TypeUtils::uintToBytes(unsigned int value, unsigned char bytes[4]) {
	bytes[0] = static_cast<unsigned char>(value & 0xFF);           // Extract least significant byte
	bytes[1] = static_cast<unsigned char>((value >> 8) & 0xFF);    // Extract second byte
	bytes[2] = static_cast<unsigned char>((value >> 16) & 0xFF);   // Extract third byte
	bytes[3] = static_cast<unsigned char>((value >> 24) & 0xFF);   // Extract most significant byte
}