#include <array>

#include "../header/CRC32.h"
// Static member initialization
std::array<unsigned int, 256> CRC32::table = CRC32::initializeTable();

// Initialize the CRC32 table
std::array<unsigned int, 256> CRC32::initializeTable()
{
	std::array<unsigned int, 256> table{};
	unsigned int polynomial = 0xEDB88320; // Standard polynomial for CRC32

	for (unsigned int i = 0; i < 256; ++i)
	{
		unsigned int crc = i;
		for (unsigned int j = 8; j > 0; --j)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ polynomial;
			}
			else
			{
				crc >>= 1;
			}
		}
		table[i] = crc;
	}

	return table;
}

unsigned int CRC32::calculate(const std::vector<unsigned char> &data)
{
	unsigned int crc = 0xFFFFFFFF;
	for (unsigned char b : data)
	{
		crc = (crc >> 8) ^ table[(crc ^ b) & 0xFF];
	}
	return crc ^ 0xFFFFFFFF;
}
