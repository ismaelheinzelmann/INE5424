#pragma once
#include <vector>
#include <netinet/in.h>
#include <sys/types.h>

#include "Datagram.h"
#ifndef PROTOCOL_H
#define PROTOCOL_H

class Protocol {
    public:
        static std::vector<unsigned char> serialize(Datagram *datagram);
        static Datagram deserialize(std::vector<unsigned char> &serializedDatagram);
        static unsigned int computeChecksum(std::vector<unsigned char> serializedDatagram);
        static bool verifyChecksum(Datagram datagram);
        static bool readDatagramSocketTimeout(Datagram& datagramBuff, int socketfd, sockaddr_in& senderAddr);
        static bool readDatagramSocket(Datagram& datagramBuff, int socketfd, sockaddr_in& senderAddr);
    private:
        static void bufferToDatagram(Datagram& datagramBuff, const std::vector<unsigned char>& bytesBuffer);

};



#endif //PROTOCOL_H
