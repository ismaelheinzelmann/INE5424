#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "../../../lib/src/header/Logger.h"
#include "../../../lib/src/header/ReliableCommunication.h"
#include <random>

#define KB 1000
#define MB 1000000
#define GB 1000000000

std::vector<unsigned char> generateRandomBytes(size_t numBytes) {
	std::vector<unsigned char> randomBytes(numBytes);
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<unsigned int> dist(0, 255);

	for (size_t i = 0; i < numBytes; ++i) {
		randomBytes[i] = static_cast<unsigned char>(dist(gen));
	}

	return randomBytes;
}

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
			std::string str(receivedMessage.second.begin(), receivedMessage.second.end());
			Logger::log("Message of "+std::to_string(receivedMessage.second.size()) + " bytes received.", LogLevel::DEBUG);
		}
	}
}

int main(int argc, char *argv[])
{
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

	std::thread printThread(print, std::ref(rb));

	while (g_running)
	{
		std::string type;
		std::cout << "Type 0 to close program or 1 to send messages for 1 minute:" << std::endl;
		std::cin >> type;
		if (type == "1") {
			auto size = 50*KB;
			auto goal = 60;
			auto message = generateRandomBytes(size);
			int sent = 0, elapsed = 0;
			auto start = std::chrono::high_resolution_clock::now();
			while (std::chrono::high_resolution_clock::now() - start < std::chrono::seconds(goal)) {
				if (!rb.sendBroadcast(message)) {
					Logger::log("Failed to send message, retrying...", LogLevel::INFO);
				} else {
					sent += size;
					int currentElapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count();

					if (currentElapsed != elapsed) {
						elapsed = currentElapsed;
						Logger::log("Elapsed time is " + std::to_string(elapsed) + " seconds", LogLevel::INFO);
						Logger::log("Output in bytes is " + std::to_string(sent), LogLevel::INFO);
					}
				}
			}
		}
		else if (type == "0") {
			std::cout << "Invalid type." << std::endl;
			break;
		}
	}
	return 0;
}
