#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <map>
#include <string>
#include <netinet/in.h>


class ConfigParser
{
public:
    ConfigParser() = delete;
    static std::map<unsigned short, sockaddr_in> parseNodes(const std::string& cleansedConfigString);

private:
    static std::string cleanFile(const std::string& configFilePath);
    static std::map<unsigned short, sockaddr_in> parseConfigurations(const std::string* nodesString, uint start);
    static void parseConfiguration(const std::string& nodeString, std::map<unsigned short, sockaddr_in>* config);
};

#endif // CONFIGPARSER_H
