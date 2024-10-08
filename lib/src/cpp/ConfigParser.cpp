#include "../header/ConfigParser.h"
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/types.h>
#define NODES_OPTION "nodes"

std::string ConfigParser::cleanFile(const std::string &configFilePath) {
	std::ifstream cf(configFilePath);
	if (cf.fail()) {
		throw std::runtime_error("Config file could not be opened.");
	}
	std::string line;
	std::string file;
	while (std::getline(cf, line)) {
		for (char character : line) {
			if (character != ' ' && character != '\n') {
				file += character;
			}
		}
	}
	return file;
}

std::map<unsigned short, sockaddr_in> ConfigParser::parseNodes(const std::string &configFilePath) {
	const std::string cleanFileString = cleanFile(configFilePath);
	const std::string searchOption = NODES_OPTION;
	const size_t nodesStartPos = cleanFileString.find(searchOption) + searchOption.length();
	// +1 considering the = sign after the declaration
	return parseConfigurations(&cleanFileString, nodesStartPos + 1);
}

std::map<unsigned short, sockaddr_in> ConfigParser::parseConfigurations(const std::string *nodesString,
																		const uint start) {
	auto nodes = std::map<unsigned short, sockaddr_in>();
	uint deep = 0, nodesPos = start, currentNodeStart = 0;
	if (nodesString == nullptr)
		throw std::runtime_error("nodesString is null");
	if (nodesString->at(nodesPos) == '{') {
		deep++;
		nodesPos++;
	}
	while (deep != 0 && nodesPos < nodesString->length()) {
		if (nodesString->at(nodesPos) == '{') {
			deep++;
			currentNodeStart = nodesPos + 1;
		}
		if (nodesString->at(nodesPos) == '}') {
			deep--;
			if (deep == 0)
				break;
			parseConfiguration(nodesString->substr(currentNodeStart, nodesPos - currentNodeStart), &nodes);
		}
		nodesPos++;
	}
	if (deep != 0)
		throw std::runtime_error("Error parsing configuration file, more information could be found at README.");

	return nodes;
}

void ConfigParser::parseConfiguration(const std::string &nodeString, std::map<unsigned short, sockaddr_in> *nodes) {
	if (nodeString.empty())
		throw std::runtime_error("nodeString is empty");
	size_t comma = 0, colon = 0, i = 0;
	while (i < nodeString.length()) {
		if (nodeString.at(i) == ':') {
			if (colon != 0)
				throw std::runtime_error(
					"Error parsing configuration file, more information could be found at README.");
			colon = i;
		}
		if (nodeString.at(i) == ',') {
			if (comma != 0)
				throw std::runtime_error(
					"Error parsing configuration file, more information could be found at README.");
			comma = i;
		}
		i++;
	}
	const int id = std::stoi(nodeString.substr(0, comma));
	const int port = std::stoi(nodeString.substr(colon + 1, nodeString.length() - colon - 1));
	if (id < 0 || port < 0)
		throw std::runtime_error("Error parsing configuration file, more information could be found at README.");
	const std::string ip = nodeString.substr(comma + 1, colon - comma - 1);
	sockaddr_in node{};
	node.sin_addr.s_addr = inet_addr(ip.c_str());
	node.sin_port = htons(port);
	node.sin_family = AF_INET;
	nodes->insert({id, node});
}

std::string ConfigParser::parseBroadcast(const std::string &configFilePath)
{
    const std::string cleanFileString = cleanFile(configFilePath);
    const std::string searchOption = "broadcast";
    const size_t broadcastStartPos = cleanFileString.find(searchOption) + searchOption.length();
    const size_t equalSignPos = cleanFileString.find('=', broadcastStartPos) + 1;
    const size_t semicolonPos = cleanFileString.find(';', equalSignPos);
    return cleanFileString.substr(equalSignPos, semicolonPos - equalSignPos);
}