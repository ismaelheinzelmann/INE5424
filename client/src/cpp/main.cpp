#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include "../../../lib/src/header/ReliableCommunication.h"

std::mutex g_lock;
bool g_running = true;

void print(ReliableCommunication &rb)
{
	while (g_running)
	{
		std::vector<unsigned char> receivedMessage = rb.receive();

		// Lock the mutex only after receiving the message
		{
			std::lock_guard guard(g_lock);
			// std::string message(receivedMessage->begin(), receivedMessage->end());
			// TODO: Modify response in the future to show who sent the message
			std::cout << "Received message: " << receivedMessage.size() << " bytes of size" << std::endl;
		}
	}
}

int main(int argc, char *argv[])
{
	// TODO Colocar recebimento de caminho para arquivo de configuração
	if (argc != 2)
	{
		std::cout << "You should inform which node you want to use." << std::endl;
		return 1;
	}

	const auto id = static_cast<unsigned short>(strtol(argv[1], nullptr, 10));
	ReliableCommunication rb("../../../config/config", id);

	rb.listen();
	std::thread printThread(print, std::ref(rb));

	while (g_running)
	{
		rb.printNodes(&g_lock);
		std::string message = std::string(), idString = std::string();
		std::cout << "Choose which node you want to send the message, or -1 to end the program:" << std::endl;
		std::cin >> idString;
		if (idString == "-1")
		{
			rb.stop();
			g_running = false;
			break;
		}
		std::cout << "Write the message:" << std::endl;
		std::cin.ignore(); // Ignore newline left in the buffer
		std::getline(std::cin, message);
		std::vector<unsigned char> messageBytes(message.begin(), message.end());
		std::string resp = rb.send(static_cast<unsigned short>(strtol(idString.c_str(), nullptr, 10)), messageBytes)
			? "Message sent successfully."
			: "Failed sending message.";
		std::cout << resp << std::endl;
	}

	if (printThread.joinable())
	{
		printThread.join();
	}

	return 0;
}
