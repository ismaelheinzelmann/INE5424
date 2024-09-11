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
      	void setSourcePort(unsigned int port);
      	void setDestinationPort(unsigned int port);
      	void setVersion(unsigned int version);
      	void setDatagramTotal(unsigned int datagramTotal);
      	void setDataLength(unsigned int dataLength);
      	void setAcknowledgement(bool acknowledgement);
      	void setChecksum(unsigned int checksum);
    	void setData(std::vector<unsigned char> data);

        unsigned int getSourcePort();
        unsigned int getDestinationPort();
        unsigned int getVersion();
        unsigned int getDatagramTotal();
        unsigned int getDataLength();
        bool getAcknowledgement();
        unsigned int getChecksum();
        std::vector<unsigned char> getData();

    private:
        unsigned int sourcePort;
        unsigned int destinPort;
        unsigned int version; // Current datagram of the message
        unsigned int datagramTotal; // How many datagrams compose the message
        unsigned int dataLength; // Length of the current datagram
        bool acknowledge; // If acknowledge, the first byte of data is the aknowledge number (which datagram is confirming), the rest can be discarded.
        unsigned int checksum; // Apply constant mask for the field at the checksum generation, using CRC32.
        std::vector<unsigned char> data;
};

#endif //DATAGRAM_H
