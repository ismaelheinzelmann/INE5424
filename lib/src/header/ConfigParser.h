#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <string>
#include "Config.h"
#include <vector>


class ConfigParser {
public:
    ConfigParser() = delete;
    static std::vector<Config> parseNodes(const std::string& cleansedConfigString);
private:
	static std::string cleanFile(const std::string& configFilePath);
    static std::vector<Config> parseConfigurations(const std::string* nodesString, uint start);
    static Config parseConfiguration(const std::string& nodeString);
};

#endif // CONFIGPARSER_H
