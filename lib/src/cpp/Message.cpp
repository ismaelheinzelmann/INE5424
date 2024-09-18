#include <chrono>
#include "../header/Message.h"
#include "../header/Datagram.h"
Message::Message(unsigned short totalDataBytes, unsigned short totalDatagrams) {
    lastUpdate = std::chrono::system_clock::now();
    data = std::vector<unsigned char>();
    lastVersionReceived = 0;
    this->totalDataBytes = totalDataBytes;
    this->totalDatagrams = totalDatagrams;
}

void Message::addData(std::vector<unsigned char> &data) {
    this->data.insert(this->data.end(), data.begin(), data.end());
    incrementVersion();
    lastUpdate = std::chrono::system_clock::now();
}

bool Message::verifyMessage(Datagram &datagram) const {
    const unsigned short datagramVersion = datagram.getVersion();

    return (datagramVersion == lastVersionReceived || datagramVersion == lastVersionReceived + 1) &&
        data.size() + datagram.getDataLength() <= totalDataBytes;
}

void Message::incrementVersion() {
    lastVersionReceived++;
}