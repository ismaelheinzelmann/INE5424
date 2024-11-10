#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include "BroadcastType.h"
#include <map>
#include <netinet/in.h>
#include <string>


class ConfigParser
{
public:
    ConfigParser() = delete;
    static std::map<unsigned short, sockaddr_in> parseNodes(const std::string& cleansedConfigString);
    static BroadcastType parseBroadcast(const std::string &cleansedConfigString);
	static std::pair<int, int> parseFaults(const std::string &configFilePath);

private:
    static std::string cleanFile(const std::string& configFilePath);
    static std::map<unsigned short, sockaddr_in> parseConfigurations(const std::string* nodesString, uint start);
    static void parseConfiguration(const std::string& nodeString, std::map<unsigned short, sockaddr_in>* config);
};

#endif // CONFIGPARSER_H
