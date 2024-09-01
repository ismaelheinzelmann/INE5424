#include "../header/ConfigParser.h"
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include<string>
#define NODES_OPTION "nodes"

std::string ConfigParser::cleanFile(const std::string& configFilePath){
    std::ifstream cf(configFilePath);
    if (cf.fail()){
        throw std::runtime_error("Config file could not be opened.");
    }
    auto configs = std::map<std::string, Config>();
    std::string line;
    std::string file;
    while (std::getline(cf, line)){
        for (char character : line){
            if (character != ' ' && character != '\n')
            {
                file += character;
            }
        }
    }
    return file;
}

std::vector<Config> ConfigParser::parseNodes(const std::string& configFilePath)
{
    const std::string cleanFileString = cleanFile(configFilePath);
    const std::string searchOption = NODES_OPTION;
    const size_t nodesStartPos = cleanFileString.find(searchOption) + searchOption.length();
    // +1 considering the = sign after the declaration
    return parseConfigurations(&cleanFileString, nodesStartPos + 1);
}

std::vector<Config> ConfigParser::parseConfigurations(const std::string* nodesString, const uint start){
    auto configs = std::vector<Config>();
    uint deep = 0, nodesPos = start, currentNodeStart = 0;
    if (nodesString == nullptr) throw std::runtime_error("nodesString is null");
    if (nodesString->at(nodesPos) == '{'){
        deep++;
        nodesPos++;
    }
    while (deep != 0 && nodesPos < nodesString->length())
    {
        if (nodesString->at(nodesPos) == '{')
        {
            deep++;
            currentNodeStart = nodesPos + 1;
        }
        if (nodesString->at(nodesPos) == '}')
        {
            deep--;
            if (deep == 0) break;
            Config config = parseConfiguration(nodesString->substr(currentNodeStart, nodesPos - currentNodeStart));
            configs.push_back(config);
        }
        nodesPos++;
    }
    if (deep != 0)
        throw std::runtime_error("Error parsing configuration file, more information could be found at README.");

    return configs;
}

Config ConfigParser::parseConfiguration(const std::string& nodeString)
{
    if (nodeString.empty()) throw std::runtime_error("nodeString is empty");
    int comma = 0, colon = 0;
    size_t i = 0;
    while (i < nodeString.length())
    {
        if (nodeString.at(i) == ':')
        {
            if (colon != 0) throw std::runtime_error("Error parsing configuration file, more information could be found at README.");
            colon = i;
        }
        if (nodeString.at(i) == ',')
        {
            if (comma != 0) throw std::runtime_error("Error parsing configuration file, more information could be found at README.");
            comma = i;
        }
        i++;
    }
    const int id = std::stoi(nodeString.substr(0, comma));
    const int port = std::stoi(nodeString.substr(colon + 1, nodeString.length() - colon - 1));
    if (id < 0 || port < 0) throw std::runtime_error("Error parsing configuration file, more information could be found at README.");
    const std::string ip = nodeString.substr(comma + 1, colon - comma - 1);
    return Config{static_cast<uint>(id),static_cast<uint>(port), ip};
}
