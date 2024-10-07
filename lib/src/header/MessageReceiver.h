#pragma once
#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H
#include <condition_variable>

#include "BlockingQueue.h"

#include "Datagram.h"
#include "Message.h"
#include <map>
#include <vector>
#include <shared_mutex>
#include <thread>

#include "Request.h"

// Thread that verifies every N seconds and remove requisitions that timedout;
class MessageReceiver
{
public:
    MessageReceiver(BlockingQueue<std::pair<bool,std::vector<unsigned char>>>* messageQueue, BlockingQueue<Request *>* requestQueue);
    ~MessageReceiver();
    void stop();
    void handleMessage(Request *request, int socketfd);

private:
    std::map<std::pair<in_addr_t, in_port_t>, Message*> messages;
    std::shared_mutex messagesMutex;
    BlockingQueue<std::pair<bool,std::vector<unsigned char>>> *messageQueue;
    BlockingQueue<Request*> *requestQueue;
    std::thread cleanseThread;

    // Cleanse
    std::condition_variable cv;
    std::mutex mtx;
    std::atomic<bool> running{true};

    void handleFirstMessage(Request* request, int socketfd);
    void handleDataMessage(Request * request, int socketfd);
    static bool verifyMessage(Request* request);
    static std::pair<in_addr_t, in_port_t> getIdentifier(sockaddr_in* from);
    static bool sendDatagramSYNACK(Datagram* datagram, sockaddr_in* from, int socketfd);
    Message* getMessage(sockaddr_in* from);
    static bool sendDatagramACK(Request* request, int socketfd);
    static bool sendDatagramNACK(Request* request, int socketfd);
    static bool sendDatagramFINACK(Request* request, int socketfd);
    static bool sendDatagramFIN(Request* request, int socketfd);
    void cleanse();


};


#endif //MESSAGEHANDLER_H
