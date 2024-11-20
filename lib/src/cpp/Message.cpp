#include "../header/Message.h"
#include <chrono>
#include "../header/Datagram.h"
#include <ranges>
Message::Message(unsigned short totalDatagrams) {
	lastUpdate = std::chrono::system_clock::now();
	this->totalDatagrams = totalDatagrams;
	data = new std::vector<unsigned char>(totalDatagrams * 1024);
	for (int i = 1; i < totalDatagrams + 1; ++i) {
		versions[i] = false;
	}
}

Message::~Message() { delete data; }

bool Message::faultyACK() {
	unsigned long finAcks = 0;
	for (const auto ack : acks | std::views::values)
		if (ack)
			finAcks++;
	const unsigned int f = acks.size() - finAcks;
	return finAcks >= 2 * f + 1;
}

bool Message::allACK() {
	for (const auto ack : acks | std::views::values)
		if (!ack)
			return false;
	return true;
}

std::shared_mutex *Message::getMutex() { return &messageMutex; }

// Adds data to the message. Returns true if ended the receive.
bool Message::addData(Datagram *datagram) {
	if (sent || delivered)
		return true;
	if (versions[datagram->getVersion()])
		return false;
	std::ranges::copy(*datagram->getData(), data->begin() + (datagram->getVersion() - 1) * 1024);
	versions[datagram->getVersion()] = true;
	if (datagram->getVersion() == totalDatagrams) {
		if (datagram->getData()->size() < 1024) {
			data->resize((totalDatagrams - 1) * 1024 + datagram->getData()->size());
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

unsigned Message::getMajorityOrder(unsigned short membersSize) {
	if (order.size() != membersSize) {
		return 0;
	}
	std::map<unsigned, unsigned> voteCount;
	for (const auto& entry : order) {
		voteCount[entry.second]++;
	}
	unsigned majority = (membersSize / 2) + 1;
	for (const auto& [vote, count] : voteCount) {
		if (count >= majority) {
			return vote;
		}
	}
	return 0;
}
