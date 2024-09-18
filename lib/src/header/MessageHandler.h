#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H
#include "Datagram.h"
#include "Message.h"
#include <map>
#include <vector>
#include <shared_mutex>
#include <netinet/in.h>

// Thread that verifies every N seconds and remove requisitions that timedout;
class MessageHandler {
    public:
        MessageHandler();
        ~MessageHandler() = default;
        void handleMessage(Datagram *datagram, sockaddr_in *from, int socketfd);
        void handleFirstMessage(Datagram *datagram, sockaddr_in *from, int socketfd);
        void handleDataMessage(Datagram *datagram, sockaddr_in *from, int socketfd);
    private:
        std::map<std::pair<in_addr_t, in_port_t>, Message*> messages;
        std::shared_mutex mutex;

        std::pair<in_addr_t, in_port_t> getIdentifier(sockaddr_in *from);
        Message* getMessage(sockaddr_in *from);
        bool sendDatagramACK(Datagram * datagram, sockaddr_in *from, int socketfd);
        bool sendDatagramNACK(Datagram * datagram, sockaddr_in *from, int socketfd);

};



#endif //MESSAGEHANDLER_H
