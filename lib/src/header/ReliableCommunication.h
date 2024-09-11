
#include <vector>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "Datagram.h"
#ifndef RELIABLE_H
#define RELIABLE_H



class ReliableCommunication {
    public:
        ReliableCommunication(std::string configFilePath, unsigned short nodeID);
        void send(unsigned short id, std::vector<unsigned char> data);
//        std::vector<unsigned char> receive();
    private:
        unsigned short id;
        std::map<unsigned short, sockaddr_in> configMap;

        Datagram createFirstDatagram(unsigned short dataLength);
        unsigned short calculateTotalDatagrams(unsigned int dataLength);
};



#endif //RELIABLE_H
