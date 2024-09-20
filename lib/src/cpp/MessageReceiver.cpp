#include "../header/MessageReceiver.h"

#include <cassert>

#include "../header/Datagram.h"
#include <map>
#include <shared_mutex>
#include "../header/Protocol.h"

MessageReceiver::MessageReceiver(std::counting_semaphore<>* sem, std::mutex *messageLock, std::queue<std::vector<unsigned char>*>* messages){
	this->messages = std::map<std::pair<in_addr_t, in_port_t>, Message *>();
	this->semaphore = sem;
	this->receivedMessages = messages;
	this->receivedMessagesMutex = messageLock;

}

void MessageReceiver::handleMessage(Datagram *datagram, sockaddr_in *from, int socketfd)
{
	if (datagram->isSYN())
	{
		handleFirstMessage(datagram, from, socketfd);
	}
	else if ((datagram->isACK() && datagram->isSYN()) || datagram->isFIN())
	{}
	else
	{
		handleDataMessage(datagram, from, socketfd);
	}
}


// Create message in messages
// Return ack for the zero datagram
// TODO destrutor
void MessageReceiver::handleFirstMessage(Datagram *datagram, sockaddr_in *from, int socketfd)
{
	auto *message = new Message(datagram->getDatagramTotal());
	from->sin_port = datagram->getSourcePort();
	auto identifier = getIdentifier(from);
	std::lock_guard lock(messagesMutex);
	messages[identifier] = message;
	sendDatagramACK(datagram, from, socketfd);
}

void MessageReceiver::handleDataMessage(Datagram *datagram, sockaddr_in *from, int socketfd)
{
	from->sin_port = datagram->getSourcePort();
	std::shared_lock lock(messagesMutex);
	Message *message = getMessage(from);
	if (message == nullptr)
	{
		sendDatagramFIN(datagram, from, socketfd);
	}
	assert(message != nullptr);

	if (message->verifyMessage(*datagram))
	{
		bool sent = message->addData(datagram->getData());
		if (sent)
		{
			sendDatagramFINACK(datagram, from, socketfd);
			std::lock_guard messagesLock(*receivedMessagesMutex);
			this->receivedMessages->push(message->getData());
			semaphore->release(1);
			return;
		}
		sendDatagramACK(datagram, from, socketfd);
	}
	else
	{
		sendDatagramNACK(datagram, from, socketfd);
	}
}


// Should be used with read lock.
Message *MessageReceiver::getMessage(sockaddr_in *from)
{
	std::pair<in_addr_t, in_port_t> identifier = getIdentifier(from);
	if (messages.find(identifier) == messages.end())
	{
		return nullptr;
	}
	return messages[identifier];
}

std::pair<in_addr_t, in_port_t> MessageReceiver::getIdentifier(sockaddr_in *from)
{
	in_addr_t fromIP = from->sin_addr.s_addr;
	in_port_t fromPort = from->sin_port;
	return std::make_pair(fromIP, fromPort);
}

bool MessageReceiver::sendDatagramACK(Datagram *datagram, sockaddr_in *from, int socketfd)
{
	auto datagramACK = Datagram();
	datagramACK.setVersion(datagram->getVersion());
	Flags flags;
	flags.ACK = true;
	Protocol::setFlags(&datagramACK, &flags);
	return Protocol::sendDatagram(datagram, from, socketfd, &flags);
}

bool MessageReceiver::sendDatagramNACK(Datagram *datagram, sockaddr_in *from, int socketfd)
{
	auto datagramNACK = Datagram();
	datagramNACK.setVersion(datagram->getVersion());
	Flags flags;
	flags.NACK = true;
	Protocol::setFlags(&datagramNACK, &flags);
	return Protocol::sendDatagram(datagram, from, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFIN(Datagram *datagram, sockaddr_in *from, int socketfd)
{
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(datagram->getVersion());
	Flags flags;
	flags.FIN = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(datagram, from, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFINACK(Datagram *datagram, sockaddr_in *from, int socketfd)
{
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(datagram->getVersion());
	Flags flags;
	flags.FIN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(datagram, from, socketfd, &flags);
}