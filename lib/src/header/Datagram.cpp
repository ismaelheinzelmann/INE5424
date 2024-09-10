//
// Created by ismael on 08/09/24.
//

#include "Datagram.h"

Datagram::Datagram() {
    this->sourcePort = 0;
    this->destinPort = 0;
    this->version = 0;
    this->datagramTotal = 0;
    this->dataLength = 0;
    this->acknowledge = false;
    for (int i = 0; i < 16; i++) {
        this->checksum[i] = std::byte(0);
    }
}

void Datagram::setSourcePort(unsigned int port) {
    this->sourcePort = port;
}

void Datagram::setDestinationPort(unsigned int port) {
    this->destinPort = port;
}

void Datagram::setVersion(unsigned int version) {
    this->version = version;
}

void Datagram::setDatagramTotal(unsigned int datagramTotal) {
    this->datagramTotal = datagramTotal;
}

void Datagram::setDataLength(unsigned int dataLength) {
    this->dataLength = dataLength;
}

void Datagram::setAcknowledgement(bool acknowledgement) {
    this->acknowledge = acknowledgement;
}

void Datagram::setChecksum(std::byte checksum[16]) {
    for (int i = 0; i < 16; i++) {
        this->checksum[i] = checksum[i];
    }
}

void Datagram::setData(std::vector<std::byte> data) {
    this->data = data;
}

unsigned int Datagram::getSourcePort() {
    return this->sourcePort;
}

unsigned int Datagram::getDestinationPort() {
    return this->destinPort;
}

unsigned int Datagram::getVersion() {
    return this->version;
}

unsigned int Datagram::getDatagramTotal() {
    return this->datagramTotal;
}

unsigned int Datagram::getDataLength() {
    return this->dataLength;
}

bool Datagram::getAcknowledgement() {
    return this->acknowledge;
}

std::byte* Datagram::getChecksum() {
    return this->checksum;
}

std::vector<std::byte> Datagram::getData() {
    return this->data;
}

std::vector<std::byte> Datagram::serialize() {
    std::vector<std::byte> serializedDatagram;
    serializedDatagram.push_back(std::byte(this->sourcePort));
    serializedDatagram.push_back(std::byte(this->destinPort));
    serializedDatagram.push_back(std::byte(this->version));
    serializedDatagram.push_back(std::byte(this->datagramTotal));
    serializedDatagram.push_back(std::byte(this->dataLength));
    serializedDatagram.push_back(std::byte(this->acknowledge));
    for (int i = 0; i < 16; i++) {
        serializedDatagram.push_back(this->checksum[i]);
    }
    for (int i = 0; i < this->data.size(); i++) {
        serializedDatagram.push_back(this->data[i]);
    }
    return serializedDatagram;
}

void Datagram::deserialize(std::vector<std::byte> serializedDatagram) {
    this->sourcePort = int(serializedDatagram[0]);
    this->destinPort = int(serializedDatagram[1]);
    this->version = int(serializedDatagram[2]);
    this->datagramTotal = int(serializedDatagram[3]);
    this->dataLength = int(serializedDatagram[4]);
    this->acknowledge = bool(serializedDatagram[5]);
    for (int i = 0; i < 16; i++) {
        this->checksum[i] = serializedDatagram[6 + i];
    }
    for (int i = 0; i < this->dataLength; i++) {
        this->data.push_back(serializedDatagram[22 + i]);
    }
}
