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
#include <cerrno>
#include <cstdio>
#include <fcntl.h>


#define BYTE_TIMEOUT_SEC 0 // Timeout duration in seconds
#define BYTE_TIMEOUT_USEC 500000 // Timeout duration in microseconds (500 ms)

// Serializes data and computes the checksum while doing so.
std::vector<unsigned char> Protocol::serialize(Datagram *datagram) {
    // TODO Refatorar
    std::vector<unsigned char> serializedDatagram;
    unsigned char buff[2] = {0, 0};

    unsigned short temp = datagram->getSourcePort();
    buff[0] = static_cast<unsigned char>(temp);
    for (unsigned char i : buff) serializedDatagram.push_back(i);

    temp =  datagram->getDestinationPort();
    buff[0] = static_cast<unsigned char>(temp);
    for (unsigned char i : buff) serializedDatagram.push_back(i);
    temp = datagram->getVersion();
    buff[0] = static_cast<unsigned char>(temp);
    for (unsigned char i : buff) serializedDatagram.push_back(i);

    temp = datagram->getDatagramTotal();
    buff[0] = static_cast<unsigned char>(temp);
    for (unsigned char i : buff) serializedDatagram.push_back(i);
    temp = datagram->getDataLength();
    buff[0] = static_cast<unsigned char>(temp);
    for (unsigned char i : buff) serializedDatagram.push_back(i);
    temp = datagram->getFlags();
    buff[0] = static_cast<unsigned char>(temp);
    for (unsigned char i : buff) serializedDatagram.push_back(i);

    for (unsigned short i = 0; i < 4; i++) serializedDatagram.push_back(0);
    for (unsigned int i = 0; i < datagram->getDataLength(); i++) serializedDatagram.push_back(datagram->getData()[i]);
    unsigned char checksum[4] = {0, 0, 0, 0};
    TypeUtils::uintToBytes(computeChecksum(serializedDatagram), checksum);
    for (unsigned short i = 0; i < 4; i++) serializedDatagram[12 + i] = checksum[i];
    return serializedDatagram;
}

// Deserializes data and returns a Datagram object.
Datagram Protocol::deserialize(std::vector<unsigned char> &serializedDatagram) {
    Datagram datagram;
    bufferToDatagram(datagram, serializedDatagram);
    std::vector<unsigned char> data;
    for (unsigned int i = 0; i < datagram.getDataLength(); i++) {
        data.push_back(serializedDatagram[i + 16]);
    }
    datagram.setData(data);
    return datagram;
}

// Computes the checksum of a datagram, the checksum field will be zero while computing.
unsigned int Protocol::computeChecksum(std::vector<unsigned char> serializedDatagram) {
    // Remove checksum from serialized datagram
    serializedDatagram.erase(serializedDatagram.begin() + 12, serializedDatagram.begin()+15);
    unsigned int checksum = CRC32::calculate(serializedDatagram);
    return checksum;
}

bool Protocol::verifyChecksum(Datagram datagram) {
    return datagram.getChecksum() == computeChecksum(serialize(&datagram));
}


bool Protocol::readDatagramSocketTimeout(Datagram& datagramBuff, int socketfd, sockaddr_in& senderAddr) {
    // Create a buffer to receive data
    auto bytesBuffer = std::vector<unsigned char>(1040);

    // Set up file descriptor set and timeout
    fd_set read_fds;
    timeval timeout{};
    timeout.tv_sec = BYTE_TIMEOUT_SEC;
    timeout.tv_usec = BYTE_TIMEOUT_USEC;

    // Initialize the file descriptor set
    FD_ZERO(&read_fds);
    FD_SET(socketfd, &read_fds);

    // Wait for data to be available on the socket
    int select_result = select(socketfd + 1, &read_fds, nullptr, nullptr, &timeout);

    if (select_result < 0) {
        // Error occurred in select()
        perror("select");
        return false;
    }
    if (select_result == 0) {
        // Timeout occurred
        std::cerr << "Timeout occurred\n";
        return false;
    }

    // Check if the socket is readable
    if (FD_ISSET(socketfd, &read_fds)) {
        ssize_t bytes_received = recvfrom(socketfd, bytesBuffer.data(), bytesBuffer.size(), 0, reinterpret_cast<struct sockaddr*>(&senderAddr), nullptr);
        if (bytes_received < 0) {
            // Error occurred in recvfrom()
            perror("recvfrom");
            return false;
        }

        // Process the received data
        bufferToDatagram(datagramBuff, bytesBuffer);
        // Ensure we don't go out of bounds
        if (bytes_received > 16) {
            const std::vector<unsigned char> dataVec(
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

bool Protocol::readDatagramSocket(Datagram& datagramBuff, int socketfd, sockaddr_in& senderAddr){
    auto bytesBuffer = std::vector<unsigned char>(1040);
    std::memset(&senderAddr, 0, sizeof(senderAddr));
    senderAddr.sin_family = AF_INET;
    socklen_t senderAddrLen = sizeof(senderAddr);
    ssize_t bytes_received = recvfrom(socketfd, bytesBuffer.data(), bytesBuffer.size(), 0, reinterpret_cast<struct sockaddr*>(&senderAddr), &senderAddrLen);
    if (bytes_received < 0) return false;
    bufferToDatagram(datagramBuff, bytesBuffer);
    const std::vector dataVec(bytesBuffer.begin() + 16, bytesBuffer.begin() + 16 + datagramBuff.getDataLength());
    datagramBuff.setData(dataVec);
    return true;
}

void Protocol::bufferToDatagram(Datagram& datagramBuff, const std::vector<unsigned char>& bytesBuffer){
    datagramBuff.setSourcePort(TypeUtils::buffToUnsignedShort(bytesBuffer, 0));
    datagramBuff.setDestinationPort(TypeUtils::buffToUnsignedShort(bytesBuffer, 2));
    datagramBuff.setVersion(TypeUtils::buffToUnsignedShort(bytesBuffer, 4));
    datagramBuff.setDatagramTotal(TypeUtils::buffToUnsignedShort(bytesBuffer, 6));
    datagramBuff.setDataLength(TypeUtils::buffToUnsignedShort(bytesBuffer, 8));
    datagramBuff.setFlags(TypeUtils::buffToUnsignedShort(bytesBuffer, 10));
    datagramBuff.setChecksum(TypeUtils::buffToUnsignedInt(bytesBuffer, 12));
}
