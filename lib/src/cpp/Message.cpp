#include "../header/Message.h"
#include <chrono>
#include "../header/Datagram.h"
#include <ranges>
Message::Message(unsigned short totalDatagrams) {
	lastUpdate = std::chrono::system_clock::now();
	this->totalDatagrams = totalDatagrams;
	data = new std::vector<unsigned char>(totalDatagrams * 2024);
	for (int i = 1; i < totalDatagrams + 1; ++i) {
		versions[i] = false;
	}
}

Message::~Message() { delete data; }

bool Message::allACK(unsigned short groupSize) {
	unsigned long finAcks = 0;
	for (const auto ack : acks | std::views::values)
		if (ack)
			finAcks++;
	const unsigned int f = groupSize - finAcks;
	return finAcks >= 2 * f + 1;
}

bool Message::messageACK() {
	unsigned long finAcks = 0;
	for (const auto ack : acks | std::views::values)
		if (ack)
			finAcks++;
	const unsigned int f = acks.size() - finAcks;
	return finAcks >= 2 * f + 1;
}


std::shared_mutex *Message::getMutex() { return &messageMutex; }

// Adds data to the message. Returns true if ended the receive.
bool Message::addData(Datagram *datagram) {
	if (sent || delivered)
		return true;
	if (versions[datagram->getVersion()])
		return false;
	std::ranges::copy(*datagram->getData(), data->begin() + (datagram->getVersion() - 1) * 2024);
	versions[datagram->getVersion()] = true;
	if (datagram->getVersion() == totalDatagrams) {
		if (datagram->getData()->size() < 2024) {
			data->resize((totalDatagrams - 1) * 2024 + datagram->getData()->size());
		}
	}
	lastUpdate = std::chrono::system_clock::now();
	if (verifyDatagrams()) {
		this->sent = true;
		return true;
	}
	return false;
}
bool Message::verifyDatagrams() {
	for (unsigned int i = 1; i <= this->totalDatagrams; i++) {
		if (!versions[i])
			return false;
	}
	return true;
}

std::vector<unsigned char> *Message::getData() const { return data; }

bool Message::verifyMessage(Datagram &datagram) const {
	const unsigned short datagramVersion = datagram.getVersion();
	if (datagramVersion > totalDatagrams) {
		return false;
	}
	return true;
}

void Message::incrementVersion() { lastUpdate = std::chrono::system_clock::now(); }

std::chrono::system_clock::time_point Message::getLastUpdate() { return lastUpdate; }
