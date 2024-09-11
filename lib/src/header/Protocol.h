#include <vector>
#include "Datagram.h"
#ifndef PROTOCOL_H
#define PROTOCOL_H

class Protocol {
    public:
        std::vector<unsigned char> serialize(Datagram &datagram);
        Datagram deserialize(std::vector<unsigned char> &serializedDatagram);
        unsigned int computeChecksum(Datagram &datagram);
        bool verifyChecksum(Datagram datagram);
};



#endif //PROTOCOL_H
