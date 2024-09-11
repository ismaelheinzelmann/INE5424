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
    serializedDatagram.push_back(datagram.getFlags());
    for (unsigned short i = 0; i < 4; i++) {
        serializedDatagram.push_back(0);
    }
    for (unsigned int i = 0; i < datagram.getDataLength(); i++) {
        serializedDatagram.push_back(datagram.getData()[i]);
    }
    serializedDatagram[12] = computeChecksum(datagram);
    return serializedDatagram;
}

// Deserializes data and returns a Datagram object.
Datagram Protocol::deserialize(std::vector<unsigned char> &serializedDatagram) {
    Datagram datagram;
    datagram.setSourcePort((unsigned short)serializedDatagram[0]);
    datagram.setDestinationPort((unsigned short)serializedDatagram[2]);
    datagram.setVersion((unsigned short)serializedDatagram[4]);
    datagram.setDatagramTotal((unsigned short)serializedDatagram[6]);
    datagram.setDataLength((unsigned short)serializedDatagram[8]);
    datagram.setFlags((unsigned short)serializedDatagram[10]);
    datagram.setChecksum((unsigned int)serializedDatagram[12]);

    std::vector<unsigned char> data;
    for (unsigned int i = 0; i < datagram.getDataLength(); i++) {
        data.push_back(serializedDatagram[i + 16]);
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
