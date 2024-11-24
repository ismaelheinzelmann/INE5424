
#include "Datagram.h"

Datagram::Datagram() {
	this->sourceAddress = 0;
	this->destinAddress = 0;
	this->sourcePort = 0;
	this->destinPort = 0;
	this->version = 0;
	this->datagramTotal = 0;
	this->dataLength = 0;
	this->flags = 0;
	this->checksum = 0;
	this->data = std::vector<unsigned char>();
}

Datagram::Datagram(Datagram *datagram) {
	setSourceAddress(datagram->getSourceAddress());
	setSourcePort(datagram->getSourcePort());
	setDestinAddress(datagram->getDestinAddress());
	setDestinationPort(datagram->getDestinationPort());
	setVersion(datagram->getVersion());
	setDatagramTotal(datagram->getDatagramTotal());
	setDataLength(datagram->getDataLength());
	setFlags(datagram->getFlags());
	setChecksum(datagram->getChecksum());

	auto dataPtr = datagram->getData();
	getData()->reserve(dataPtr->size());
	for (size_t i = 0; i < dataPtr->size(); ++i) {
		(*getData())[i] = dataPtr->at(i);
	}
}

void Datagram::setSourceAddress(unsigned int address) { this->sourceAddress = address; }

void Datagram::setSourcePort(unsigned short port) { this->sourcePort = port; }

void Datagram::setDestinAddress(unsigned int address) { this->destinAddress = address; }

void Datagram::setDestinationPort(unsigned short port) { this->destinPort = port; }

void Datagram::setVersion(unsigned short version) { this->version = version; }

void Datagram::setDatagramTotal(unsigned short datagramTotal) { this->datagramTotal = datagramTotal; }

void Datagram::setDataLength(unsigned short dataLength) { this->dataLength = dataLength; }

void Datagram::setFlags(unsigned short flags) { this->flags = flags; }

void Datagram::setChecksum(unsigned int checksum) { this->checksum = checksum; }

void Datagram::setData(std::vector<unsigned char> data) { this->data = data; }

unsigned int Datagram::getSourceAddress() { return this->sourceAddress; }

unsigned short Datagram::getSourcePort() { return this->sourcePort; }

unsigned int Datagram::getDestinAddress() { return this->destinAddress; }

unsigned short Datagram::getDestinationPort() { return this->destinPort; }

unsigned short Datagram::getVersion() { return this->version; }

unsigned short Datagram::getDatagramTotal() { return this->datagramTotal; }

unsigned short Datagram::getDataLength() { return this->dataLength; }

unsigned short Datagram::getFlags() { return this->flags; }

unsigned int Datagram::getChecksum() { return this->checksum; }

std::vector<unsigned char> *Datagram::getData() { return &this->data; }

bool Datagram::isBitSet(unsigned short value, int bitPosition) {
	short bitmask = 1 << bitPosition;
	return (value & bitmask) != 0;
}

unsigned short Datagram::setBit(unsigned short value, int bitPosition) {
	short bitmask = 1 << bitPosition;
	return value | bitmask;
}

bool Datagram::isACK() { return this->isBitSet(this->getFlags(), 0); }

bool Datagram::isSYN() { return this->isBitSet(this->getFlags(), 1); }

bool Datagram::isFIN() { return this->isBitSet(this->getFlags(), 3); }

bool Datagram::isBROADCAST() { return this->isBitSet(this->getFlags(), 4); }

bool Datagram::isHEARTBEAT() { return this->isBitSet(this->getFlags(), 5); }

bool Datagram::isJOIN() { return this->isBitSet(this->getFlags(), 6); }

bool Datagram::isSYNCHRONIZE() { return this->isBitSet(this->getFlags(), 7); }

bool Datagram::isEND() { return this->isBitSet(this->getFlags(), 10); }

void Datagram::setIsACK() { this->setFlags(this->setBit(this->getFlags(), 0)); }

void Datagram::setIsSYN() { this->setFlags(this->setBit(this->getFlags(), 1)); }

void Datagram::setIsFIN() { this->setFlags(this->setBit(this->getFlags(), 3)); }

void Datagram::setIsBROADCAST() { this->setFlags(this->setBit(this->getFlags(), 4)); }

void Datagram::setIsHEARTBEAT() { this->setFlags(this->setBit(this->getFlags(), 5)); }

void Datagram::setIsJOIN() { this->setFlags(this->setBit(this->getFlags(), 6)); }

void Datagram::setIsSYNCHRONIZE() { this->setFlags(this->setBit(this->getFlags(), 7)); }

void Datagram::setIsEND() { this->setFlags(this->setBit(this->getFlags(), 10)); }
