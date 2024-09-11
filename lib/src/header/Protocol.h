#include <vector>
#include "Datagram.h"
#ifndef PROTOCOL_H
#define PROTOCOL_H

class Protocol {
    public:
        static std::vector<unsigned char> serialize(Datagram *datagram);
        static Datagram deserialize(std::vector<unsigned char> &serializedDatagram);
        static unsigned int computeChecksum(std::vector<unsigned char> serializedDatagram);
        static bool verifyChecksum(Datagram datagram);
};



#endif //PROTOCOL_H
