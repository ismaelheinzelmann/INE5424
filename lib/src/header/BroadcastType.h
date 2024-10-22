#pragma once
#include <string>
#ifndef BROADCASTTYPE_H
#define BROADCASTTYPE_H
typedef enum BroadcastType { BEB, URB, AB } BroadcastType;

inline std::string BroadcastTypeToString(BroadcastType broadcastType) {
	switch (broadcastType) {
	case BEB:
		return "BEB";
	case URB:
		return "URB";
	case AB:
		return "AB";
	}
	return "";
}
#endif //BROADCASTTYPE_H
