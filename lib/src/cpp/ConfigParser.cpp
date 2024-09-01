#include "../header/ConfigParser.h"
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
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
    //TODO: Parse node string to struct
    std::cout<<nodeString<<std::endl;
    return {};
}
