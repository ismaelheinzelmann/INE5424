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
    if (datagram->getVersion() == 0){
        handleFirstMessage(datagram, from, socketfd);
//    } else {
//        handleDataMessage(datagram, from, socketfd);
//    }
    }
}


// Create message in messages
// Return ack for the zero datagram
void MessageHandler::handleFirstMessage(Datagram *datagram, sockaddr_in *from, int socketfd){
    Message message = Message(datagram->getDataLength(), datagram->getDatagramTotal());
    auto identifier = getIdentifier(from);
    std::lock_guard<std::shared_mutex> lock(mutex);
    messages[identifier] = &message;
    sendDatagramACK(datagram, from, socketfd);
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