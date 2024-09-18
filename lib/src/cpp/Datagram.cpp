
#include "../header/Datagram.h"
Datagram::Datagram() {
    this->sourcePort = 0;
    this->destinPort = 0;
    this->version = 0;
    this->datagramTotal = 0;
    this->dataLength = 0;
    this->flags = 0;
    this->checksum = 0;
    this->data = std::vector<unsigned char>();
}

void Datagram::setSourcePort(unsigned short port) {
    this->sourcePort = port;
}

void Datagram::setDestinationPort(unsigned short port) {
    this->destinPort = port;
}

void Datagram::setVersion(unsigned short version) {
    this->version = version;
}

void Datagram::setDatagramTotal(unsigned short datagramTotal) {
    this->datagramTotal = datagramTotal;
}

void Datagram::setDataLength(unsigned short dataLength) {
    this->dataLength = dataLength;
}

void Datagram::setFlags(unsigned short flags) {
    this->flags = flags;
}

void Datagram::setChecksum(unsigned int checksum) {
    this->checksum = checksum;
}

void Datagram::setData(std::vector<unsigned char> data) {
    this->data = data;
}

unsigned short Datagram::getSourcePort() {
    return this->sourcePort;
}

unsigned short Datagram::getDestinationPort() {
    return this->destinPort;
}

unsigned short Datagram::getVersion() {
    return this->version;
}

unsigned short Datagram::getDatagramTotal() {
    return this->datagramTotal;
}

unsigned short Datagram::getDataLength() {
    return this->dataLength;
}

unsigned short Datagram::getFlags() {
    return this->flags;
}

unsigned int Datagram::getChecksum() {
    return this->checksum;
}

std::vector<unsigned char> Datagram::getData() {
    return this->data;
}

bool Datagram::isBitSet(unsigned short value, int bitPosition) {
    short bitmask = 1 << bitPosition;
    return (value & bitmask) != 0;
}

unsigned short Datagram::setBit(unsigned short value, int bitPosition) {
    short bitmask = 1 << bitPosition;
    return value | bitmask;
}

bool Datagram::isACK(){
    return this->isBitSet(this->getFlags(), 0);
}

bool Datagram::isSYN() {
     return this->isBitSet(this->getFlags(), 1);
}

bool Datagram::isNACK() {
     return this->isBitSet(this->getFlags(), 2);
}

void Datagram::setIsACK(){
    this->setFlags(this->setBit(this->getFlags(), 0));
}

void Datagram::setIsSYN(){
    this->setFlags(this->setBit(this->getFlags(), 1));
}
void Datagram::setIsNACK(){
	this->setFlags(this->setBit(this->getFlags(), 2));
}