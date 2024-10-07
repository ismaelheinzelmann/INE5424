#pragma once
#include <chrono>
#include <map>

#include <mutex>
#include <vector>
#include "Datagram.h"
#ifndef MESSAGE_H
#define MESSAGE_H

class Message {
public:
	explicit Message(unsigned short totalDatagrams);
	~Message();
	bool addData(Datagram *datagram);
	bool verifyDatagrams();
	bool verifyMessage(Datagram &datagram) const;
	std::mutex *getMutex();
	std::chrono::system_clock::time_point getLastUpdate();
	std::vector<unsigned char> *getData() const;
	bool sent = false;
	bool delivered = false;

private:
	void incrementVersion();
	std::chrono::system_clock::time_point lastUpdate;
	std::vector<unsigned char> *data;
	unsigned short totalDatagrams;
	std::mutex messageMutex;
	std::map<unsigned short, bool> versions;
};


#endif // MESSAGE_H
