//
// Created by ismael on 08/09/24.
//
#include <cstddef>
#include <vector>
#ifndef DATAGRAM_H
#define DATAGRAM_H
class Datagram {
    public:
      	Datagram();
      	void setSourcePort(unsigned int port);
      	void setDestinationPort(unsigned int port);
      	void setVersion(unsigned int version);
      	void setDatagramTotal(unsigned int datagramTotal);
      	void setDataLength(unsigned int dataLength);
      	void setAcknowledgement(bool acknowledgement);
      	void setChecksum(std::byte checksum[16]);
    	void setData(std::vector<std::byte> data);

        unsigned int getSourcePort();
        unsigned int getDestinationPort();
        unsigned int getVersion();
        unsigned int getDatagramTotal();
        unsigned int getDataLength();
        bool getAcknowledgement();
        std::byte* getChecksum();
        std::vector<std::byte> getData();
        std::vector<std::byte> serialize();
        void deserialize(std::vector<std::byte> serializedDatagram);

    private:
        unsigned int sourcePort;
        unsigned int destinPort;
        unsigned int version; // Current datagram of the message
        unsigned int datagramTotal; // How many datagrams compose the message
        unsigned int dataLength; // Length of the current datagram
        bool acknowledge; // If acknowledge, the first byte of data is the aknowledge number (which datagram is confirming), the rest can be discarded.
        std::byte checksum[16]; // Apply constant mask for the field at the checksum generation, using MD5
        std::vector<std::byte> data;
};

#endif //DATAGRAM_H
