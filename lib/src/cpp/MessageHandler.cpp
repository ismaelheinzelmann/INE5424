#include "../header/MessageHandler.h"
#include "../header/Datagram.h"
#include <map>
#include <shared_mutex>
#include <stdexcept>
#include <sys/socket.h>
#include "../header/Protocol.h"
MessageHandler::MessageHandler(){
	messages = std::map<std::pair<in_addr_t, in_port_t>, Message*>();
}

void MessageHandler::handleMessage(Datagram *datagram, sockaddr_in *from, int socketfd) {
    if (datagram->isSYN()) {
        handleFirstMessage(datagram, from, socketfd);
    } else if ((datagram->isACK() && datagram->isSYN()) || datagram->isFIN()) {
    } else {
        handleDataMessage(datagram, from, socketfd);
    }
}


// Create message in messages
// Return ack for the zero datagram
// TODO destrutor
void MessageHandler::handleFirstMessage(Datagram *datagram, sockaddr_in *from, int socketfd){
    auto *message = new Message(datagram->getDatagramTotal());
    from->sin_port = datagram->getSourcePort();
    auto identifier = getIdentifier(from);
    std::lock_guard lock(mutex);
    messages[identifier] = message;
    sendDatagramACK(datagram, from, socketfd);
}

void MessageHandler::handleDataMessage(Datagram *datagram, sockaddr_in *from, int socketfd){
    from->sin_port = datagram->getSourcePort();
    std::shared_lock lock(mutex);
    Message *message = getMessage(from);
    if (message == nullptr) {
        throw std::runtime_error("Message not found");
    }

    if (message->verifyMessage(*datagram)) {
        message->addData(datagram->getData());
        sendDatagramACK(datagram, from, socketfd);
    } else {
        sendDatagramNACK(datagram, from, socketfd);
    }
}


// Should be used with read lock.
Message *MessageHandler::getMessage(sockaddr_in *from) {
    std::pair<in_addr_t, in_port_t> identifier = getIdentifier(from);
    if (messages.find(identifier) == messages.end()) {
        return nullptr;
    };
    return messages[identifier];
}
std::pair<in_addr_t, in_port_t> MessageHandler::getIdentifier(sockaddr_in *from) {
    in_addr_t fromIP = from->sin_addr.s_addr;
    in_port_t fromPort = from->sin_port;
    return std::make_pair(fromIP, fromPort);
}

bool MessageHandler::sendDatagramACK(Datagram * datagram, sockaddr_in *from, int socketfd) {
    auto datagramACK = Datagram();
    datagramACK.setVersion(datagram->getVersion());
    datagramACK.setIsACK();
    datagramACK.setIsSYN();
    auto serializedDatagramACK = Protocol::serialize(&datagramACK);
    const ssize_t bytes = sendto(socketfd, serializedDatagramACK.data(), serializedDatagramACK.size(), 0,
                                 reinterpret_cast<struct sockaddr *>(from), sizeof(*from));
    if (bytes == static_cast<ssize_t>(serializedDatagramACK.size())) {
        return true;
    }
    return false;
}

bool MessageHandler::sendDatagramNACK(Datagram * datagram, sockaddr_in *from, int socketfd) {
    auto datagramNACK = Datagram();
    datagramNACK.setVersion(datagram->getVersion());
    datagramNACK.setIsNACK();
    auto serializedDatagramNACK = Protocol::serialize(&datagramNACK);
    const ssize_t bytes = sendto(socketfd, serializedDatagramNACK.data(), serializedDatagramNACK.size(), 0,
                                 reinterpret_cast<struct sockaddr *>(from), sizeof(*from));
    if (bytes == static_cast<ssize_t>(serializedDatagramNACK.size())) {
        return true;
    }
    return false;
}