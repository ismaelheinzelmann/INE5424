#include "../header/Message.h"
#include "../header/Datagram.h"
#include <chrono>

#include <iostream>
Message::Message(unsigned short totalDatagrams) {
    lastUpdate = std::chrono::system_clock::now();
    data = std::vector<unsigned char>();
    lastVersionReceived = 0;
    this->totalDatagrams = totalDatagrams;
}

void Message::addData(std::vector<unsigned char> *data) {
  std::cout << std::string(data->begin(), data->end()) << std::endl;
    this->data.insert(this->data.end(), data->begin(), data->end());
    incrementVersion();
    lastUpdate = std::chrono::system_clock::now();
}

bool Message::verifyMessage(Datagram &datagram) const {
    const unsigned short datagramVersion = datagram.getVersion();
    if (datagramVersion != this->lastVersionReceived && datagramVersion != lastVersionReceived + 1) {
        return false;
    }
    if (datagramVersion == totalDatagrams &&
        datagram.getData()->size() != datagram.getDataLength()) {
        return false;
    }
    return true;
}

void Message::incrementVersion() {
    lastVersionReceived++;
}