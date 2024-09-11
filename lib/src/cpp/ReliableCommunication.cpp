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

void ReliableCommunication::send(unsigned short id, const std::vector<unsigned char>& data) {
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
                                 reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    if (bytes < 0) {
        throw std::runtime_error("Could not send data.");
    }
    // TODO: Implementar lógica pos SYN

}

Datagram ReliableCommunication::createFirstDatagram(unsigned short dataLength){
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

unsigned short ReliableCommunication::calculateTotalDatagrams(unsigned int dataLength){
      double result = static_cast<double>(dataLength) / 16.384;
      return static_cast<int>(ceil(result));
}


//std::vector<unsigned char> ReliableCommunication::receive(){
//    return;
//};