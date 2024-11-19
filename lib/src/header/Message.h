#pragma once
#include <chrono>
#include <map>

#include <mutex>
#include <vector>
#include "Datagram.h"

#include <shared_mutex>
#ifndef MESSAGE_H
#define MESSAGE_H

class Message {
public:
	explicit Message(unsigned short totalDatagrams);
	~Message();
	bool addData(Datagram *datagram);
	bool verifyDatagrams();
	bool verifyMessage(Datagram &datagram) const;
	std::shared_mutex *getMutex();
	std::chrono::system_clock::time_point getLastUpdate();
	std::vector<unsigned char> *getData() const;
	bool sent = false;
	bool delivered = false;
	bool broadcastMessage = false;
	std::map<std::pair<unsigned int, unsigned short>, bool> acks;
	bool allACK();
	bool faultyACK();


private:
	std::shared_mutex messageMutex;
	void incrementVersion();
	std::chrono::system_clock::time_point lastUpdate;
	std::vector<unsigned char> *data;
	unsigned short totalDatagrams;
	std::map<unsigned short, bool> versions;
};


#endif // MESSAGE_H
