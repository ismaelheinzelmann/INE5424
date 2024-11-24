#include "../header/ConfigParser.h"

#include "BroadcastType.h"
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

BroadcastType ConfigParser::parseBroadcast(const std::string &configFilePath)
{
    const std::string cleanFileString = cleanFile(configFilePath);
    const std::string searchOption = "broadcast";
    const size_t broadcastStartPos = cleanFileString.find(searchOption) + searchOption.length();
    const size_t equalSignPos = cleanFileString.find('=', broadcastStartPos) + 1;
    const size_t semicolonPos = cleanFileString.find(';', equalSignPos);
	auto type = cleanFileString.substr(equalSignPos, semicolonPos - equalSignPos);
	if (type == "URB") {
		return URB;
	}
	if (type == "AB") {
		return AB;
	}
    return BEB;
}

std::pair<int, int> ConfigParser::parseFaults(const std::string &configFilePath) {
	const std::string cleanFileString = cleanFile(configFilePath);
	const std::string searchOption = "faults";
	const size_t faultsStartPos = cleanFileString.find(searchOption) + searchOption.length();
	const size_t start = cleanFileString.find("{{", faultsStartPos) + 2;
	const size_t end = cleanFileString.find("}}", start);

	// Extracting the substring with drop and corrupt settings
	std::string faultsSubstr = cleanFileString.substr(start, end - start);

	// Variables to store drop and corrupt values
	int drop = 0, corrupt = 0;

	// Finding drop value
	const size_t dropPos = faultsSubstr.find("drop=");
	if (dropPos != std::string::npos) {
		const size_t dropStart = dropPos + 5;
		const size_t dropEnd = faultsSubstr.find("}", dropStart);
		drop = std::stoi(faultsSubstr.substr(dropStart, dropEnd - dropStart));
	}

	// Finding corrupt value
	const size_t corruptPos = faultsSubstr.find("corrupt=");
	if (corruptPos != std::string::npos) {
		const size_t corruptStart = corruptPos + 8;
		const size_t corruptEnd = faultsSubstr.find("}", corruptStart);
		corrupt = std::stoi(faultsSubstr.substr(corruptStart, corruptEnd - corruptStart));
	}

	return {drop, corrupt};
}

int ConfigParser::parseKeepAlive(const std::string &configFilePath) {
	const std::string cleanFileString = cleanFile(configFilePath);
	const std::string searchOption = "alive";
	const size_t heartbeatStartPos = cleanFileString.find(searchOption) + searchOption.length();
	const size_t equalSignPos = cleanFileString.find('=', heartbeatStartPos) + 1;
	const size_t semicolonPos = cleanFileString.find(';', equalSignPos);
	return std::stoi(cleanFileString.substr(equalSignPos, semicolonPos - equalSignPos));
}