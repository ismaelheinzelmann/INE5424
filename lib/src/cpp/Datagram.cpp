//
// Created by ismael on 08/09/24.
//

#include "../header/Datagram.h"

Datagram::Datagram() {
    this->sourcePort = 0;
    this->destinPort = 0;
    this->version = 0;
    this->datagramTotal = 0;
    this->dataLength = 0;
    this->acknowledge = false;
    this->checksum = 0;
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

void Datagram::setChecksum(unsigned int checksum) {
    this->checksum = checksum;
}

void Datagram::setData(std::vector<unsigned char> data) {
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

unsigned int Datagram::getChecksum() {
    return this->checksum;
}

std::vector<unsigned char> Datagram::getData() {
    return this->data;
}
