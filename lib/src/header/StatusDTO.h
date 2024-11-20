#include "map"
#include "mutex"
#include "NodeStatus.h"
#ifndef STATUSDTO_H
#define STATUSDTO_H
typedef struct StatusDTO {
	std::map<std::pair<unsigned int, unsigned short>, NodeStatus> *nodeStatus;
	std::map<std::pair<unsigned int, unsigned short>, std::pair<std::pair<unsigned int, unsigned short>, unsigned int>> *order;
	std::map<std::pair<unsigned int, unsigned short>, std::chrono::system_clock::time_point> *heartbeatsTimes;
	std::shared_mutex *heartbeatsLock;
} statusDTO;

#endif //STATUSDTO_H
