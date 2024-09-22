#include "../header/Message.h"
#include "../header/Datagram.h"
#include <chrono>

#include <iostream>

Message::Message(unsigned short totalDatagrams)
{
	lastUpdate = std::chrono::system_clock::now();
	lastVersionReceived = 0;
	this->totalDatagrams = totalDatagrams;
	data = new std::vector<unsigned char>();
}

Message::~Message()
{
	if (!delivered)
	{
		delete data;
	}
}

//Adds data to the message. Returns true if ended the receive.
bool Message::addData(Datagram *datagram)
{
	if (datagram->getVersion() <= lastVersionReceived) return false;
	if (sent || delivered) return true;
	this->data->insert(this->data->end(), datagram->getData()->begin(), datagram->getData()->end());
	incrementVersion();
	lastUpdate = std::chrono::system_clock::now();
	const bool sent = lastVersionReceived == totalDatagrams;
	if (sent)
	{
		this->sent = true;
		return true;
	}
	return false;
}

std::vector<unsigned char> *Message::getData() const
{
	return data;
}

bool Message::verifyMessage(Datagram &datagram) const
{
	const unsigned short datagramVersion = datagram.getVersion();
	// if (datagramVersion != this->lastVersionReceived && datagramVersion != lastVersionReceived + 1)
	if (datagramVersion > lastVersionReceived + 1 || datagramVersion > totalDatagrams)
	{
		return false;
	}
	// if (datagramVersion == totalDatagrams &&
	// 	datagram.getData()->size() != datagram.getDataLength())
	// {
	// 	return false;
	// }
	return true;
}

void Message::incrementVersion()
{
	lastVersionReceived++;
	lastUpdate = std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point Message::getLastUpdate()
{
	return lastUpdate;
}
