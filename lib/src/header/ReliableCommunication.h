#pragma once
#include "Datagram.h"
#include <bits/std_mutex.h>
#include <map>
#include <netinet/in.h>
#include <string>
#include <vector>
#include "MessageHandler.h"
#ifndef RELIABLE_H
#define RELIABLE_H



class ReliableCommunication {
    public:
        ReliableCommunication(std::string configFilePath, unsigned short nodeID);
        ~ReliableCommunication();
        void printNodes(std::mutex *lock) const;
        bool send(unsigned short id, const std::vector<unsigned char> &data);
        bool ackAttempts(int transientSocketfd, sockaddr_in &destin,
                         std::vector<unsigned char> &serializedData) const;
        bool dataAttempts(int transientSocketfd, sockaddr_in &destin,
                          std::vector<unsigned char> &serializedData) const;
        bool dataAttempt(int transientSocketfd, sockaddr_in &destin,
                         std::vector<unsigned char> &serializedData) const;
        bool ackAttempt(int transientSocketfd, sockaddr_in &destin,
                        std::vector<unsigned char> &serializedData) const;
        bool sendAttempt(int socketfd, sockaddr_in &destin,
                         std::vector<unsigned char> &serializedData,
                         Datagram &datagram, int retryMS) const;
        std::vector<unsigned char> receive();
        void receiveAndPrint(std::mutex* lock);
        static std::pair<int, sockaddr_in> createUDPSocketAndGetPort();

    private:
        unsigned short id;
        std::map<unsigned short, sockaddr_in> configMap;
        int socketInfo;
        MessageHandler *handler;
        bool verifyOrigin(sockaddr_in &senderAddr);
        static Datagram createFirstDatagram(unsigned short dataLength);
        static Datagram createAckDatagram(unsigned short dataLength);
        static unsigned short calculateTotalDatagrams(unsigned int dataLength);
};



#endif //RELIABLE_H
