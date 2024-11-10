#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include "../../../lib/src/header/ReliableCommunication.h"
#include "../../../lib/src/header/Logger.h"
#include "../../../lib/src/header/BroadcastType.h"

LogLevel Logger::current_log_level = LogLevel::INFO;
int main(int argc, char *argv[])
{
	// TODO Colocar recebimento de caminho para arquivo de configuração
	if (argc < 2)
	{
		std::cout << "You should inform which node you want to use." << std::endl;
		return 1;
	}
	if (argc > 2)
	{
		Logger::setLogLevel(std::string(argv[2]));
	}
	const auto id = static_cast<unsigned short>(strtol(argv[1], nullptr, 10));
	ReliableCommunication rb("../../../config/config", id);

	rb.listen();

	while (true)
	{
		std::string type;
		std::cout << "Message type: 1 for unicast test and 2 for broadcast test" << std::endl;
		std::cin >> type;
		 if (type == "-1") {
		 	std::exit(0);
		 }
		if (type == "1") {
			std::string idString = std::string();
			std::cout << "Choose which node you want to send the message:" << std::endl;
			std::cin >> idString;
			std::cin.ignore(); // Remove this if necessary
			int success = 0;
			for (int i = 0; i < 100; i++) {
				std::string message = "Node: " + std::to_string(strtol(argv[1], nullptr, 10)) + " | Message: " + std::to_string(i);
				std::vector<unsigned char> messageBytes(message.begin(), message.end());

				bool sent = rb.send(strtol(argv[1], nullptr, 10), messageBytes);
				if (sent)
					success++;
			}
			std::cout << "Success in " + std::to_string(success) + " of 100 messages." << std::endl;
			std::cout<< "Messages contents:"<<std::endl;
			if (idString == std::to_string(strtol(argv[1], nullptr, 10))){
				for (int i = 0; i < success; i++) {
					auto receivedMessage = rb.receive();
					std::string str(receivedMessage.second.begin(), receivedMessage.second.end());
					std::cout<<str<<std::endl;
				}
			}
		}
		else if (type == "2") {
			int success = 0;
			for (int i = 0; i < 100; i++) {
				std::string message = "Node: " + std::to_string(strtol(argv[1], nullptr, 10)) + " | Message: " + std::to_string(i);
				std::vector<unsigned char> messageBytes(message.begin(), message.end());
				bool sent = rb.sendBroadcast(messageBytes);
				if (sent){
					success++;
				} else {
				std::cout<<"FALHOU"<<std::endl;
				}
			}
			std::cout << "Success in " + std::to_string(success) + " of 100 messages." << std::endl;
			std::cout<< "Messages contents:"<<std::endl;
			for (int i = 0; i < success; i++) {
				auto receivedMessage = rb.receive();
				std::string str(receivedMessage.second.begin(), receivedMessage.second.end());
				std::cout<<str<<std::endl;
			}
		}
		else {
			std::cout << "Invalid type." << std::endl;
		}
	}
	return 0;
}
