#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H
#include "Datagram.h"
#include "Message.h"
#include <map>
#include <queue>
#include <vector>
#include <shared_mutex>
#include <netinet/in.h>
#include <semaphore>

// Thread that verifies every N seconds and remove requisitions that timedout;
class MessageReceiver
{
public:
    MessageReceiver(std::counting_semaphore<>* sem, std::mutex *messageLock, std::queue<std::vector<unsigned char>*>* messages);
    ~MessageReceiver() = default;
    void handleMessage(Datagram* datagram, sockaddr_in* from, int socketfd);
    void handleFirstMessage(Datagram* datagram, sockaddr_in* from, int socketfd);
    void handleDataMessage(Datagram* datagram, sockaddr_in* from, int socketfd);
    std::vector<unsigned char> *getReceivedData();

private:
    std::map<std::pair<in_addr_t, in_port_t>, Message*> messages;
    std::shared_mutex messagesMutex;

    std::queue<std::vector<unsigned char>*> *receivedMessages;
    std::mutex *receivedMessagesMutex;
    std::counting_semaphore<> *semaphore;


        static std::pair<in_addr_t, in_port_t> getIdentifier(sockaddr_in* from);
    Message* getMessage(sockaddr_in* from);
    static bool sendDatagramACK(Datagram* datagram, sockaddr_in* from, int socketfd);
    static bool sendDatagramNACK(Datagram* datagram, sockaddr_in* from, int socketfd);
    static bool sendDatagramFINACK(Datagram *datagram, sockaddr_in *from, int socketfd);
    static bool sendDatagramFIN(Datagram* datagram, sockaddr_in* from, int socketfd);

};


#endif //MESSAGEHANDLER_H
