#include "Datagram.h"
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef SOCKETREADER_H
#define SOCKETREADER_H



class SocketReader {
    public:
        SocketReader();
        ~SocketReader();
        void send(std::vector<unsigned char> data);
        void receive();
        Datagram readDatagram();
    private:

};



#endif //SOCKETREADER_H
