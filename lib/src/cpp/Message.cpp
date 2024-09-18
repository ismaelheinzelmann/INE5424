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
}

bool Message::verifyMessage(Datagram &datagram) {
    unsigned short datagramVersion = datagram.getVersion();
    if (datagramVersion == lastVersionReceived || datagramVersion == lastVersionReceived + 1) {
        incrementVersion();
        lastUpdate = std::chrono::system_clock::now();
        // if (data.size() + datagram.getDataLength() == totalDataBytes) {
        //     return true;
        // }
        return true;
    }

    return false;
}

void Message::incrementVersion() {
    lastVersionReceived++;
}