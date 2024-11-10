#include <vector>
#ifndef FAULTINJECTOR_H
#define FAULTINJECTOR_H



class FaultInjector {
	public:
		static bool returnTrueByChance(int chance);
		static void corruptVector(std::vector<unsigned char>* data);
};



#endif //FAULTINJECTOR_H
