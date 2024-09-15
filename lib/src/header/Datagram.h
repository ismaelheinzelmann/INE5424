#pragma once
#include <vector>
#ifndef DATAGRAM_H
#define DATAGRAM_H
// Datagram class for socket comunication.
// The datagram is the unit of data that is sent through the socket.
// The datagram is composed by the following fields:
// - Source port: The port of the sender.
// - Destination port: The port of the receiver.
// - Version: The current datagram of the message.
// - Datagram total: How many datagrams compose the message.
// - Data length: Length of the current datagram.
// - Acknowledgement: If acknowledge, the first byte of data is the aknowledge number (which datagram is confirming), the rest can be discarded.
// - Checksum: Apply constant mask for the field at the checksum generation, using CRC32.
// - Data: The data of the datagram.
class Datagram {
    public:
      	Datagram();
      	void setSourcePort(unsigned short port);
      	void setDestinationPort(unsigned short port);
      	void setVersion(unsigned short version);
      	void setDatagramTotal(unsigned short datagramTotal);
      	void setDataLength(unsigned short dataLength);
		void setFlags(unsigned short flags);
      	void setChecksum(unsigned int checksum);
    	void setData(std::vector<unsigned char> data);

        unsigned short getSourcePort();
        unsigned short getDestinationPort();
        unsigned short getVersion();
        unsigned short getDatagramTotal();
        unsigned short getDataLength();
		unsigned short getFlags();
        unsigned int getChecksum();
        std::vector<unsigned char> getData();

        // Flag based
      	bool isACK(); // First bit
        bool isSYN(); // Second bit
		bool isNACK();// Third bit

        void setIsACK();
        void setIsSYN();
		void setIsNACK();

		bool isBitSet(unsigned short value, int bitPosition);
		unsigned short setBit(unsigned short value, int bitPosition);

    private:
        unsigned short sourcePort;
        unsigned short destinPort;
        unsigned short version; // Current datagram of the message
        unsigned short datagramTotal; // How many datagrams compose the message
        unsigned short dataLength; // Length of the current datagram
        unsigned short flags; // Mask of flags
        unsigned int checksum; // Apply constant mask for the field at the checksum generation, using CRC32.
        std::vector<unsigned char> data;
};

#endif //DATAGRAM_H
