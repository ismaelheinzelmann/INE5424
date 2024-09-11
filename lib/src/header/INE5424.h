//
// Created by ismael on 11/09/24.
//
#include <vector>
#ifndef INE5424_H
#define INE5424_H



class INE5424 {
    public:
        void send(unsigned int id, std::vector<unsigned char> data);
        std::vector<unsigned char> receive();
};



#endif //INE5424_H
