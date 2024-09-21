#pragma once
#include <chrono>
#include "Datagram.h"
#include <vector>
#ifndef MESSAGE_H
#define MESSAGE_H

class Message
{
public:
    explicit Message(unsigned short totalDatagrams);
    ~Message();
    bool addData(std::vector<unsigned char>* data);
    bool verifyMessage(Datagram& datagram) const;
    std::vector<unsigned char> *getData() const;
    bool sent = false;

private:
    void incrementVersion();
    std::chrono::system_clock::time_point lastUpdate;
    std::vector<unsigned char> *data;
    unsigned short lastVersionReceived;
    unsigned short totalDatagrams;
};


#endif //MESSAGE_H