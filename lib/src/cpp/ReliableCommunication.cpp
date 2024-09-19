#include "../header/ReliableCommunication.h"
#include "../header/ConfigParser.h"
#include "../header/Protocol.h"

#include <TypeUtils.h>
#include <cmath>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#define RETRY_ACK_ATTEMPT 4
#define RETRY_ACK_TIMEOUT_USEC 250000

#define RETRY_DATA_ATTEMPT 8
#define RETRY_DATA_TIMEOUT_USEC 250000

ReliableCommunication::ReliableCommunication(std::string configFilePath, unsigned short nodeID) {
    this->configMap = ConfigParser::parseNodes(configFilePath);
    this->id = nodeID;
    if (this->configMap.find(id) == this->configMap.end()) {
        throw std::runtime_error("Invalid ID.");
    }
    socketInfo = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketInfo < 0) {
        throw std::runtime_error("Socket could not be created.");
    }
    const sockaddr_in& addr = this->configMap[id];
    if (bind(socketInfo, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(socketInfo);
        throw std::runtime_error("Could not bind socket.");
    }
    handler = new MessageHandler();
}

ReliableCommunication::~ReliableCommunication() {
  close(socketInfo);
  delete handler;
}

bool ReliableCommunication::send(const unsigned short id,
                                 const std::vector<unsigned char> &data) {
    if (this->configMap.find(id) == this->configMap.end()) {
        throw std::runtime_error("Invalid ID.");
    }
    std::pair<int, sockaddr_in> transientSocketFd = createUDPSocketAndGetPort();

    Datagram datagram = createFirstDatagram(data.size());
    // datagram.setDataLength(data.size());
    unsigned short totalDatagrams = calculateTotalDatagrams(data.size());
    datagram.setDatagramTotal(calculateTotalDatagrams(totalDatagrams));
    // datagram.setData(data);
    datagram.setSourcePort(transientSocketFd.second.sin_port);
    std::vector<unsigned char> sendBuffer = Protocol::serialize(&datagram);
    sockaddr_in destinAddr = this->configMap[id];
    bool accepted = ackAttempts(transientSocketFd.first, destinAddr, sendBuffer);
    if (!accepted) {
        return false;
    }
    // Verify if its faster to pre compute serialization
    for (unsigned short i = 0; i < totalDatagrams; i++) {
        auto versionDatagram = Datagram();
        versionDatagram.setSourcePort(transientSocketFd.second.sin_port);
        versionDatagram.setVersion(i);
        versionDatagram.setDatagramTotal(totalDatagrams);
        for (unsigned short j = 0; j < 1024; j++) {
            const unsigned int index = i * 1024 + j;
            if (index >= data.size()) break;
            versionDatagram.getData()->push_back(data.at(index));
        }
        versionDatagram.setDataLength(versionDatagram.getData()->size());
        auto serializedDatagram = Protocol::serialize(&versionDatagram);
        unsigned char checksum[4] = {0, 0, 0, 0};
        TypeUtils::uintToBytes(Protocol::computeChecksum(serializedDatagram), checksum);
        for (unsigned short j = 0; j < 4; j++) serializedDatagram[12 + j] = checksum[j];
        Datagram response;
        for (int j = 0; j < RETRY_DATA_ATTEMPT; ++j) {
            const ssize_t bytes = sendto(this->socketInfo, serializedDatagram.data(), serializedDatagram.size(), 0,
                                                 reinterpret_cast<struct sockaddr *>(&destinAddr), sizeof(destinAddr));
            if (bytes < 0) {
                return false;
            }
            sockaddr_in senderAddr{};
            senderAddr.sin_family = AF_INET;
            bool sent = Protocol::readDatagramSocketTimeout(response, transientSocketFd.first, senderAddr, RETRY_DATA_TIMEOUT_USEC + RETRY_DATA_TIMEOUT_USEC * j);
            // bool sent = Protocol::readDatagramSocket(response, transientSocketFd.first, senderAddr);
            if (!sent) continue;
            // if (!Protocol::verifyChecksum(response)) continue;
            if (response.isACK()) {
                break;
            }
        }
    }
    close(transientSocketFd.first);
    return true;
}

bool ReliableCommunication::ackAttempts(int transientSocketfd, sockaddr_in &destin, std::vector<unsigned char> &serializedData) const {
    Datagram response;
    for (int i = 0; i < RETRY_ACK_ATTEMPT; ++i) {
        const ssize_t bytes = sendto(this->socketInfo, serializedData.data(), serializedData.size(), 0,
                                             reinterpret_cast<struct sockaddr *>(&destin), sizeof(destin));
        if (bytes < 0) {
            return false;
        }
        sockaddr_in senderAddr{};
        senderAddr.sin_family = AF_INET;
        bool sent = Protocol::readDatagramSocketTimeout(response, transientSocketfd, senderAddr, RETRY_ACK_TIMEOUT_USEC + RETRY_ACK_TIMEOUT_USEC * i);
        if (!sent) continue;
        // if (!Protocol::verifyChecksum(response)) continue;
        // if (response.isACK()) return true;;
        if (response.isACK() && response.isSYN()) {
            std::cout<<"FIRST ACK"<< std::endl;
            return true;
        }
    }
    return false;
}

bool ReliableCommunication::dataAttempts(int transientSocketfd, sockaddr_in &destin, std::vector<unsigned char> &serializedData) const {
    Datagram response;
    for (int i = 0; i < RETRY_DATA_ATTEMPT; ++i) {
        bool sent = sendAttempt(transientSocketfd, destin, serializedData, response, RETRY_DATA_TIMEOUT_USEC + (i * RETRY_DATA_TIMEOUT_USEC));
        if (!sent) continue;
        // if (!Protocol::verifyChecksum(response)) continue;
        if (response.isACK()) return true;
        if (response.isFIN()) return false;
    }
    return false;
}

bool ReliableCommunication::sendAttempt(int socketfd, sockaddr_in &destin, std::vector<unsigned char> &serializedData, Datagram &datagram, int retryMS) const {
    const ssize_t bytes = sendto(this->socketInfo, serializedData.data(), serializedData.size(), 0,
                                             reinterpret_cast<struct sockaddr *>(&destin), sizeof(destin));
    if (bytes < 0) {
        return false;
    }
    sockaddr_in senderAddr{};
    senderAddr.sin_family = AF_INET;
    return Protocol::readDatagramSocketTimeout(datagram, socketfd, senderAddr, retryMS);
}

std::vector<unsigned char> ReliableCommunication::receive() {
    Datagram datagram;
    sockaddr_in senderAddr{};
    senderAddr.sin_family = AF_INET;
    if (const bool received = Protocol::readDatagramSocket(datagram, socketInfo, senderAddr); !received) {
        close(socketInfo);
        throw std::runtime_error("Failed to read datagram.");
    }
    if (!verifyOrigin(senderAddr)) {
        throw std::runtime_error("Invalid sender address.");
    }
    // Extract data from the datagram

    handler->handleMessage(&datagram, &senderAddr, this->socketInfo);
    // TODO Este metodo deve ser listen, receive deve esperar o semaphoro e retornar a mensagem.
    auto data = datagram.getData();

    // Return the data
    return *data;
}

void ReliableCommunication::printNodes(std::mutex *lock) const
{
    lock->lock();
    std::cout << "Nodes:" << std::endl;
    for (auto & it : this->configMap) std::cout << it.first << std::endl;
    lock->unlock();
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
    const double result = static_cast<double>(dataLength) / 1024;
    return static_cast<int>(ceil(result));
}


void ReliableCommunication::receiveAndPrint(std::mutex *lock) {
    while (true){
        auto message = receive();
        std::string messageString(message.begin(), message.end());lock->lock();
        lock->lock();
        std::cout << messageString << std::endl;
        lock->unlock();
    }
}

bool ReliableCommunication::verifyOrigin( sockaddr_in& senderAddr){
    for (const auto & [_, nodeAddr]: this->configMap) {
        if (senderAddr.sin_addr.s_addr == nodeAddr.sin_addr.s_addr && senderAddr.sin_port == nodeAddr.sin_port){
            return true;
        }
    }
    return false;
}

std::pair<int, sockaddr_in> ReliableCommunication::createUDPSocketAndGetPort() {
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);

    // Create a UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("socket");
        throw std::runtime_error("Failed to create socket");
    }

    // Prepare sockaddr_in structure for a dummy bind
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(0); // Let the system choose an available port

    // Bind the socket to get an available port (dummy bind)
    if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        throw std::runtime_error("Failed to bind socket");
    }

    // Retrieve the assigned port
    if (getsockname(sockfd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) < 0) {
        perror("getsockname");
        close(sockfd);
        throw std::runtime_error("Failed to get socket name");
    }

    // Return the file descriptor and the address
    return {sockfd, addr};
}