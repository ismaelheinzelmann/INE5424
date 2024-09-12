#include "../header/ReliableCommunication.h"
#include "../header/ConfigParser.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>
#include <cmath>
#include "../header/Protocol.h"

ReliableCommunication::ReliableCommunication(std::string configFilePath, unsigned short nodeID) {
    this->configMap = ConfigParser::parseNodes(configFilePath);
    this->id = nodeID;
}

void ReliableCommunication::send(unsigned short id, const std::vector<unsigned char> &data) {
    const int socketInfo = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketInfo < 0) {
        throw std::runtime_error("Socket could not be created.");
    }
    if (this->configMap.find(id) == this->configMap.end()) {
        throw std::runtime_error("Invalid ID.");
    }

    Datagram datagram = createFirstDatagram(data.size());

    const std::vector<unsigned char> sendBuffer = Protocol::serialize(&datagram);

    sockaddr_in address = this->configMap[id];
    const ssize_t bytes = sendto(socketInfo, sendBuffer.data(), sendBuffer.size(), 0,
                                 reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    if (bytes < 0) {
        throw std::runtime_error("Could not send data.");
    }
    // TODO: Implementar lógica pos SYN
}

Datagram ReliableCommunication::createFirstDatagram(unsigned short dataLength) {
    auto firstDatagram = Datagram();
    firstDatagram.setSourcePort(0);
    firstDatagram.setDestinationPort(0);
    firstDatagram.setVersion(0);
    firstDatagram.setDatagramTotal(calculateTotalDatagrams(dataLength));
    firstDatagram.setDataLength(0);
    firstDatagram.setFlags(0);
    firstDatagram.setIsSYN();
    return firstDatagram;
}

Datagram ReliableCommunication::createAckDatagram(unsigned short dataLength) {
    auto ackDatagram = Datagram();
    ackDatagram.setSourcePort(0);
    ackDatagram.setDestinationPort(0);
    ackDatagram.setVersion(0);
    ackDatagram.setDatagramTotal(calculateTotalDatagrams(dataLength));
    ackDatagram.setDataLength(0);
    ackDatagram.setFlags(0);
    ackDatagram.setIsACK();
    return ackDatagram;
}

unsigned short ReliableCommunication::calculateTotalDatagrams(unsigned int dataLength) {
    double result = static_cast<double>(dataLength) / 16.384;
    return static_cast<int>(ceil(result));
}

std::vector<unsigned char> ReliableCommunication::receive() {
    const int socketInfo = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketInfo < 0) {
        throw std::runtime_error("Socket could not be created.");
    }
    if (this->configMap.find(id) == this->configMap.end()) {
        throw std::runtime_error("Invalid ID.");
    }

    int socketServer = bind(socketInfo, reinterpret_cast<sockaddr *>(&this->configMap[id]),
                            sizeof(this->configMap[id]));
    if (socketServer < 0) {
        throw std::runtime_error("Could not bind socket.");
    }

    std::vector<unsigned char> buffer[32]; // Not sure about this size
    sockaddr_in src_address{};
    socklen_t AddrSrcLen = sizeof(src_address);

    ssize_t receivedMessage = recvfrom(socketInfo, buffer, sizeof(buffer), 0,
                                       reinterpret_cast<struct sockaddr *>(&src_address), &AddrSrcLen);
    if (receivedMessage < 0) {
        throw std::runtime_error("Could not receive data.");
    }

    if (Datagram receivedDatagram = Protocol::deserialize(*buffer); receivedDatagram.isSYN()) {
        Datagram ackDatagram = createAckDatagram(buffer[0].size());
        const std::vector<unsigned char> sendBuffer = Protocol::serialize(&ackDatagram);
        const ssize_t bytes = sendto(socketInfo, sendBuffer.data(), sendBuffer.size(), 0,
                                     reinterpret_cast<struct sockaddr *>(&src_address), sizeof(src_address));
        if (bytes < 0) {
            throw std::runtime_error("Could not send data.");
        }

        return {};
    }

    return {};
};
