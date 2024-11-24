#ifndef TYPEUTILS_H
#define TYPEUTILS_H
#include <vector>


class TypeUtils {
public:
	static unsigned short buffToUnsignedShort(const std::vector<unsigned char> &buffer, unsigned int i);
	static unsigned int buffToUnsignedInt(const std::vector<unsigned char> &buffer, unsigned int i);
	static void uintToBytes(unsigned int value, std::vector<unsigned char> *bytes);
};


#endif // TYPEUTILS_H
