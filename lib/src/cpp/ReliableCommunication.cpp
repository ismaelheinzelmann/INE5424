#include "../header/ReliableCommunication.h"
#include "../header/ConfigParser.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdexcept>
#include <cmath>

ReliableCommunication::ReliableCommunication(std::string configFilePath, unsigned short nodeID) {
    this->configMap = ConfigParser::parseNodes(configFilePath);
    this->id = nodeID;
}

void ReliableCommunication::send(unsigned short id, std::vector<unsigned char> data) {
    int socketInfo = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketInfo < 0) {
        throw std::runtime_error("Socket could not be created.");
    }
    if (this->configMap.find(id) == this->configMap.end()) {
        throw std::runtime_error("Invalid ID.");
    }
    struct sockaddr_in address = *(struct sockaddr_in*)&this->configMap[id];
    if (bind(socketInfo, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Could not bind to socket.");
    }
    Datagram datagram;
    datagram = this->createFirstDatagram(data.size());
    // https://medium.com/@shahidahmadkhan86/creating-a-udp-server-in-c-a1d1fc317dd3


}

Datagram ReliableCommunication::createFirstDatagram(unsigned short dataLength){
    Datagram firstDatagram = Datagram();
    firstDatagram.setSourcePort(0);
    firstDatagram.setDestinationPort(0);
    firstDatagram.setVersion(0);
    firstDatagram.setDatagramTotal(this->calculateTotalDatagrams(dataLength));
    firstDatagram.setDataLength(dataLength);
    firstDatagram.setFlags(0);
    firstDatagram.setIsACK();
    firstDatagram.setIsFIRST();
    return firstDatagram;
}

unsigned short ReliableCommunication::calculateTotalDatagrams(unsigned int dataLength){
      double result = static_cast<double>(dataLength) / 16.384;
      return static_cast<int>(ceil(result));
}


//std::vector<unsigned char> ReliableCommunication::receive(){
//    return;
//};