#pragma once
#include <chrono>
#include "Datagram.h"
#include <vector>
#include <mutex>
#ifndef MESSAGE_H
#define MESSAGE_H

class Message
{
public:
    explicit Message(unsigned short totalDatagrams);
    ~Message();
    bool addData(Datagram *datagram);
    bool verifyMessage(Datagram& datagram) const;
    std::mutex * getMutex();
    std::chrono::system_clock::time_point getLastUpdate();
    std::vector<unsigned char> *getData() const;
    bool sent = false;
    bool delivered = false;

private:
    void incrementVersion();
    std::chrono::system_clock::time_point lastUpdate;
    std::vector<unsigned char> *data;
    unsigned short lastVersionReceived;
    unsigned short totalDatagrams;
    std::mutex messageMutex;
};


#endif //MESSAGE_H
