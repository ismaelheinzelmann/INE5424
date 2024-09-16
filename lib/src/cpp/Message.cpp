#include <chrono>
#include "../header/Message.h"
#include "../header/Datagram.h"
Message::Message(unsigned short totalDataBytes, unsigned short totalDatagrams) {
    startTime = std::chrono::system_clock::now();
    data = std::vector<unsigned char>();
    lastVersionReceived = 0;
    this->totalDataBytes = totalDataBytes;
    this->totalDatagrams = totalDatagrams;
}