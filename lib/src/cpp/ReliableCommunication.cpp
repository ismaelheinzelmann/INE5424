#include "../header/ReliableCommunication.h"
#include "../header/ConfigParser.h"
#include "../header/Protocol.h"
#include <cmath>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

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

ReliableCommunication::~ReliableCommunication()
{
    close(socketInfo);
    delete handler;
}


void ReliableCommunication::send(const unsigned short id, const std::vector<unsigned char> &data) {
    if (this->configMap.find(id) == this->configMap.end()) {
        throw std::runtime_error("Invalid ID.");
    }
    std::pair<int, sockaddr_in> transientSocketFd = createUDPSocketAndGetPort();

    Datagram datagram = createFirstDatagram(data.size());
    datagram.setDataLength(data.size());
    datagram.setDatagramTotal(calculateTotalDatagrams(data.size()));
    datagram.setData(data);
    datagram.setSourcePort(transientSocketFd.second.sin_port);

    const std::vector<unsigned char> sendBuffer = Protocol::serialize(&datagram);

    sockaddr_in destinAddr = this->configMap[id];
    const ssize_t bytes = sendto(this->socketInfo, sendBuffer.data(), sendBuffer.size(), 0,
                                 reinterpret_cast<struct sockaddr *>(&destinAddr), sizeof(destinAddr));


    if (bytes < 0) {
        throw std::runtime_error("Could not send data.");
    }
    sockaddr_in senderAddr{};
    senderAddr.sin_family = AF_INET;
    if (const bool received = Protocol::readDatagramSocket(datagram, transientSocketFd.first, senderAddr); !received) {
        close(transientSocketFd.first);
        throw std::runtime_error("Failed to read datagram.");
    }

    // TODO: Revisar flags no sendto e afins, provavelmente nem precisaremos do flags no datagram
}


std::vector<unsigned char> ReliableCommunication::receive() {
    Datagram datagram;
    sockaddr_in senderAddr{};
    senderAddr.sin_family = AF_INET;
    if (const bool received = Protocol::readDatagramSocket(datagram, socketInfo, senderAddr); !received) {
        close(socketInfo);
        throw std::runtime_error("Failed to read datagram.");
    }
    if (datagram.getVersion() == 0 && datagram.isACK() && datagram.isSYN() && !verifyOrigin(senderAddr)) {
        throw std::runtime_error("Invalid sender address.");
    }
    // Extract data from the datagram

    handler->handleMessage(&datagram, &senderAddr, this->socketInfo);
    // TODO Este metodo deve ser listen, receive deve esperar o semaphoro e retornar a mensagem.
    const std::vector<unsigned char>& data = datagram.getData();

    // Return the data
    return data;
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
    const double result = static_cast<double>(dataLength) / 16384;
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