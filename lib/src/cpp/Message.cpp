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
	if (!sent)
	{
		delete data;
	}
}

//Adds data to the message. Returns true if ended the receive.
bool Message::addData(std::vector<unsigned char> *data)
{
	std::cout << std::string(data->begin(), data->end()) << std::endl;
	this->data->insert(this->data->end(), data->begin(), data->end());
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
	if (datagramVersion != this->lastVersionReceived && datagramVersion != lastVersionReceived + 1)
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
}
