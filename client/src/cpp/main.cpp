#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "../../../lib/src/header/Logger.h"
#include "../../../lib/src/header/ReliableCommunication.h"

std::mutex g_lock;
bool g_running = true;
LogLevel Logger::current_log_level = LogLevel::INFO;

void print(ReliableCommunication &rb) {
	while (g_running) {
		auto receivedMessage = rb.receive();
		if (!receivedMessage.first)
			break;
		{
			std::lock_guard guard(g_lock);
			std::cout << "Received message: " << receivedMessage.second.size() << " bytes of size" << std::endl;
			std::string str(receivedMessage.second.begin(), receivedMessage.second.end());
			std::cout << "Message content: " << str << std::endl;
		}
	}
}

int main(int argc, char *argv[]) {
	// TODO Colocar recebimento de caminho para arquivo de configuração
	if (argc < 2) {
		std::cout << "You should inform which node you want to use." << std::endl;
		return 1;
	}
	if (argc > 2) {
		Logger::setLogLevel(std::string(argv[2]));
	}
	const auto id = static_cast<unsigned short>(strtol(argv[1], nullptr, 10));
	ReliableCommunication rb("../../../config/config", id);

	rb.listen();
	std::thread printThread(print, std::ref(rb));

	while (g_running) {
		// rb.printNodes(&g_lock);
		std::string message = std::string(), idString = std::string();
		// std::cout << "Choose which node you want to send the message, or -1 to end the program:" << std::endl;
		// std::cin >> idString;
		// if (idString == "-1")
		// {
		// 	rb.stop();
		// 	g_running = false;
		// 	break;
		// }
		std::cout << "Write the message:" << std::endl;
		// std::cin.ignore();
		std::getline(std::cin, message);
		std::vector<unsigned char> messageBytes(message.begin(), message.end());
		auto before = std::chrono::system_clock::now();
		// std::string resp = rb.send(static_cast<unsigned short>(strtol(idString.c_str(), nullptr, 10)), messageBytes)
		std::string resp = rb.sendBroadcast(messageBytes) ? "Message sent successfully." : "Failed sending message.";
		Logger::log("Time spent: " +
						std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
										   std::chrono::system_clock::now() - before)
										   .count()) +
						"ms",
					LogLevel::INFO);
		std::cout << resp << std::endl;
	}

	if (printThread.joinable()) {
		printThread.join();
	}

	return 0;
}
