//
// Created by ismael on 10/09/24.
//
#include <vector>
#include <array>

#ifndef CRC32_H
#define CRC32_H

class CRC32 {
public:
    static unsigned int calculate(const std::vector<unsigned char>& data);

private:
    static std::array<unsigned int, 256> initializeTable();
    static std::array<unsigned int, 256> table;
};



#endif //CRC32_H
