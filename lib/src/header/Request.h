#pragma once
#include <vector>
#include <netinet/in.h>
#include "Datagram.h"

#ifndef REQUEST_H
#define REQUEST_H

struct Request {
	std::vector<unsigned char> *data;
	sockaddr_in *clientRequest;
	Datagram *datagram;
};

#endif //REQUEST_H
