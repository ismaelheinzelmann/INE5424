#pragma once
#include <chrono>
#include <map>
#include <vector>
#ifndef MESSAGE_H
#define MESSAGE_H

class Message {
    public:
        Message(unsigned short totalDataBytes, unsigned short totalDatagrams);
        void addData(std::vector<unsigned char> &data);
    private:
        void incrementVersion();
        std::chrono::system_clock::time_point startTime;
        std::vector<unsigned char> data;
        unsigned short lastVersionReceived;
        unsigned short totalDataBytes;
        unsigned short totalDatagrams;
};



#endif //MESSAGE_H
