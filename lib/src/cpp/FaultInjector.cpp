#include "../header/FaultInjector.h"

#include <Logger.h>
#include <random>
bool FaultInjector::returnTrueByChance(int chance){
	if (chance < 0 || chance > 100) {
		return false;
	}

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 99);

	int randomValue = dis(gen);

	return randomValue < chance;
}

void FaultInjector::corruptVector(std::vector<unsigned char>* data) {
    if (!data || data->empty()) {
        return;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> indexDis(0, data->size() - 1);

    std::uniform_int_distribution<> numCorruptions(1, 20);
    int corruptionCount = numCorruptions(gen);

    for (int i = 0; i < corruptionCount; ++i) {
        int index = indexDis(gen);
        unsigned char originalValue = (*data)[index];
        unsigned char corruptedValue = ~originalValue;
        (*data)[index] = corruptedValue;
    }
	Logger::log("Corrupted.", LogLevel::DEBUG);
}