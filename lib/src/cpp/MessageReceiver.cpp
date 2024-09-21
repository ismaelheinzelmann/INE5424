#include "../header/MessageReceiver.h"

#include <Request.h>
#include <cassert>

#include "../header/Datagram.h"
#include <map>
#include <shared_mutex>
#include "../header/Protocol.h"
#include "../header/BlockingQueue.h"

MessageReceiver::MessageReceiver(BlockingQueue<std::vector<unsigned char> *> *messageQueue,
                                 BlockingQueue<Request *> *requestQueue)
{
	this->messages = std::map<std::pair<in_addr_t, in_port_t>, Message *>();
	this->messageQueue = messageQueue;
	this->requestQueue = requestQueue;
}


bool MessageReceiver::verifyMessage(Request *request)
{
	return Protocol::verifyChecksum(request->datagram, request->data);
}

bool returnTrueWithProbability(int n) {
	// Ensure n is between 0 and 100
	if (n < 0 || n > 100) {
		throw std::invalid_argument("Probability must be between 0 and 100.");
	}

	// Generate a random number between 0 and 99
	int randomValue = std::rand() % 100;

	// Return true if randomValue is less than n
	return randomValue < n;
}

void MessageReceiver::handleMessage(Request *request, int socketfd)
{
	if (!returnTrueWithProbability(99)) return;
	// if (!verifyMessage(request))
	// {
	// 	sendDatagramNACK(request, socketfd);
	// }
	if ((request->datagram->isACK() && request->datagram->isSYN()) || request->datagram->isFIN())
		return;
	if (request->datagram->isSYN() && !request->datagram->isACK())
	{
		handleFirstMessage(request, socketfd);
	}
	else
	{
		handleDataMessage(request, socketfd);
	}
	// }
	delete request->datagram;
	delete request->data;
	delete request->clientRequest;
	delete request;
}


// Create message in messages
// Return ack for the zero datagram
// TODO destrutor
void MessageReceiver::handleFirstMessage(Request *request, int socketfd)
{
	auto *message = new Message(request->datagram->getDatagramTotal());
	request->clientRequest->sin_port = request->datagram->getSourcePort();
	auto identifier = getIdentifier(request->clientRequest);
	std::lock_guard lock(messagesMutex);
	messages[identifier] = message;
	sendDatagramACK(request, socketfd);
}

void MessageReceiver::handleDataMessage(Request *request, int socketfd)
{
	request->clientRequest->sin_port = request->datagram->getSourcePort();
	std::shared_lock lock(messagesMutex);
	Message *message = getMessage(request->clientRequest);
	if (message == nullptr)
	{
		sendDatagramFIN(request, socketfd);
	}
	assert(message != nullptr);

	if (message->verifyMessage(*(request->datagram)))
	{
		bool sent = message->addData(request->datagram->getData());
		if (sent)
		{
			sendDatagramFINACK(request, socketfd);
			if (message->delivered) return;
			messageQueue->push(message->getData());
			message->delivered = true;
			return;
		}
		sendDatagramACK(request, socketfd);
	}
	else
	{
		sendDatagramNACK(request, socketfd);
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

bool MessageReceiver::sendDatagramACK(Request *request, int socketfd)
{
	auto datagramACK = Datagram();
	datagramACK.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.ACK = true;
	Protocol::setFlags(&datagramACK, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramNACK(Request *request, int socketfd)
{
	auto datagramNACK = Datagram();
	datagramNACK.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.NACK = true;
	Protocol::setFlags(&datagramNACK, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFIN(Request *request, int socketfd)
{
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.FIN = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}

bool MessageReceiver::sendDatagramFINACK(Request *request, int socketfd)
{
	auto datagramFIN = Datagram();
	datagramFIN.setVersion(request->datagram->getVersion());
	Flags flags;
	flags.FIN = true;
	flags.ACK = true;
	Protocol::setFlags(&datagramFIN, &flags);
	return Protocol::sendDatagram(request->datagram, request->clientRequest, socketfd, &flags);
}
