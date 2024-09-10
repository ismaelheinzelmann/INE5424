//
// Created by ismael on 10/09/24.
//

#include "../header/Protocol.h"
#include "../header/CRC32.h"

// Serializes data and computes the checksum while doing so.
std::vector<unsigned char> Protocol::serialize(Datagram &datagram) {
    std::vector<unsigned char> serializedDatagram;
    serializedDatagram.push_back(datagram.getSourcePort());
    serializedDatagram.push_back(datagram.getDestinationPort());
    serializedDatagram.push_back(datagram.getVersion());
    serializedDatagram.push_back(datagram.getDatagramTotal());
    serializedDatagram.push_back(datagram.getDataLength());
    serializedDatagram.push_back(datagram.getAcknowledgement());
    serializedDatagram.push_back(computeChecksum(datagram));

    for (unsigned int i = 0; i < datagram.getDataLength(); i++) {
        serializedDatagram.push_back(datagram.getData()[i]);
    }
    return serializedDatagram;
}

// Deserializes data and returns a Datagram object.
Datagram Protocol::deserialize(std::vector<unsigned char> &serializedDatagram) {
    Datagram datagram;
    datagram.setSourcePort((unsigned int)serializedDatagram[0]);
    datagram.setDestinationPort((unsigned int)serializedDatagram[1]);
    datagram.setVersion((unsigned int)serializedDatagram[2]);
    datagram.setDatagramTotal((unsigned int)serializedDatagram[3]);
    datagram.setDataLength((unsigned int)serializedDatagram[4]);
    datagram.setAcknowledgement((bool)serializedDatagram[5]);
    datagram.setChecksum((unsigned int)serializedDatagram[6]);

    std::vector<unsigned char> data;
    for (unsigned int i = 0; i < datagram.getDataLength(); i++) {
        data.push_back(serializedDatagram[i + 7]);
    }
    datagram.setData(data);
    return datagram;
}

// Computes the checksum of a datagram, the checksum field will be zero while computing.
unsigned int Protocol::computeChecksum(Datagram &datagram) {
    std::vector<unsigned char> serializedDatagram = serialize(datagram);
    // Remove checksum from serialized datagram
    serializedDatagram.erase(serializedDatagram.begin() + 6);
    unsigned int checksum = CRC32::calculate(serializedDatagram);
    return checksum;
}

bool Protocol::verifyChecksum(Datagram datagram) {
    return datagram.getChecksum() == computeChecksum(datagram);
}
