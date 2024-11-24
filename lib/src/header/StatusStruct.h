#include <map>
#include "NodeStatus.h"
#include <shared_mutex>
#ifndef STATUSSTRUCT_H
#define STATUSSTRUCT_H

typedef struct StatusStruct {
	std::map<std::pair<unsigned, unsigned short>, NodeStatus> nodeStatus;
	std::shared_mutex nodeStatusMutex;

} StatusStruct;
#endif //STATUSSTRUCT_H
