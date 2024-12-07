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
			Logger::log("Message of "+std::to_string(receivedMessage.second.size()) + " bytes received.", LogLevel::INFO);
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
		std::cout << "Type 0 to close program or 1 to send 1GB in messages:" << std::endl;
		std::cin >> type;
		if (type == "1") {
			int size = 0;
			auto data = std::vector<std::vector<unsigned char>>();
			while (size < GB) {
				auto randomVector = generateRandomBytes(30*KB);
				size+=MB;
				data.push_back(randomVector);
			}
			int sent = 0, percentage = 0;
			auto start = std::chrono::high_resolution_clock::now();
			for (auto message : data) {
				bool success = false;
				while (!success) {
					if (!rb.sendBroadcast(message)) {
						Logger::log("Failed to send message, retrying...", LogLevel::INFO);
					} else {
						success = true;
						sent++;
						if (static_cast<int>(sent * 100 / data.size()) != percentage) {
							percentage = sent * 100 / data.size();
							Logger::log("Message progress is now " + std::to_string(percentage) + "%", LogLevel::INFO);
						}

					}
				}
			}

			auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start);
			Logger::log("Execution time is " + std::to_string(duration.count())+" seconds.", LogLevel::INFO);


		}
		else if (type == "0") {
			std::cout << "Invalid type." << std::endl;
			break;
		}
	}
	return 0;
}
