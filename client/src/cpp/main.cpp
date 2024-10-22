#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include "../../../lib/src/header/ReliableCommunication.h"
#include "../../../lib/src/header/Logger.h"
#include "../../../lib/src/header/BroadcastType.h"

std::mutex g_lock;
bool g_running = true;
LogLevel Logger::current_log_level = LogLevel::INFO;

void print(ReliableCommunication &rb)
{
	while (g_running)
	{
		auto receivedMessage = rb.receive();
		if (!receivedMessage.first) break;
		{
			std::lock_guard guard(g_lock);
			std::cout << "Received message: " << receivedMessage.second.size() << " bytes of size" << std::endl;
			std::string str(receivedMessage.second.begin(), receivedMessage.second.end());
			std::cout<<str<<std::endl;
			std::cout << "Message content: " << str << std::endl;
		}
	}
}

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
	std::thread printThread(print, std::ref(rb));

	while (g_running)
	{
		std::string type;
		std::cout << "Message type: 1 for unicast and 2 for broadcast" << std::endl;
		std::cin >> type;
		if (type == "1")
		{
			std::string idString = std::string();
			rb.printNodes(&g_lock);
			std::cout << "Choose which node you want to send the message:" << std::endl;
			std::cin >> idString;
			std::cin.ignore(); // Remove this if necessary
			std::string message = std::string();
			std::cout << "Write the message:" << std::endl;
			std::getline(std::cin, message);
			std::vector<unsigned char> messageBytes(message.begin(), message.end());
			auto before = std::chrono::system_clock::now();
			std::string resp = rb.send(static_cast<unsigned short>(strtol(idString.c_str(), nullptr, 10)), messageBytes)
								? "Message sent successfully."
								: "Failed sending message.";
			Logger::log("Time spent: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - before).count()) + "ms", LogLevel::INFO);
			std::cout << resp << std::endl;
		}
		else if (type == "2")
		{
			for (int i = 0; i < 5;i++) {

				std::string message = "NODE: " + std::to_string(id) + " MESSAGE: " + std::to_string(i);
				std::vector<unsigned char> messageBytes(message.begin(), message.end());
				rb.sendBroadcast(messageBytes);
			}
			// std::string bcType = BroadcastTypeToString(rb.getBroadcastType());
			// std::cout << "Broadcast type: " << bcType << std::endl;
			// std::cin.ignore(); // Remove this if necessary
			// std::string message = std::string();
			// std::cout << "Write the message:" << std::endl;
			// std::getline(std::cin, message);
			// std::vector<unsigned char> messageBytes(message.begin(), message.end());
			// auto before = std::chrono::system_clock::now();
			// std::string resp = rb.sendBroadcast(messageBytes)
			// 					? "Message sent successfully."
			// 					: "Failed sending message.";
			// Logger::log("Time spent: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - before).count()) + "ms", LogLevel::INFO);
			// std::cout << resp << std::endl;
		}
		else
		{
			std::cout << "Invalid type." << std::endl;
			break;
		}
	}


	if (printThread.joinable())
	{
		printThread.join();
	}

	return 0;
}
