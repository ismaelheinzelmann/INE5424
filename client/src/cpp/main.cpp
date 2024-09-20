#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include "../../../lib/src/header/ReliableCommunication.h"

std::mutex g_lock;
bool g_running = true;

void receive(ReliableCommunication& rb) {
	while (g_running) {
		rb.listen();
	}
}

void print(ReliableCommunication& rb) {
	while (g_running) {
		std::vector<unsigned char>* receivedMessage = rb.receive();

		std::lock_guard guard(g_lock);
		std::string message(receivedMessage->begin(), receivedMessage->end());
		// TODO Modificar resposta futuramente para mostrar quem enviou a mensagem
		std::cout << "Received message " << ": " << message << std::endl;
		delete receivedMessage;

	}
}

int main(int argc, char* argv[]) {
	// TODO Colocar recebimento de caminho para arquivo de configuração
	if (argc != 2) {
		std::cout << "You should inform which node you want to use." << std::endl;
		return 1;
	}

	const auto id = static_cast<unsigned short>(strtol(argv[1], nullptr, 10));
	ReliableCommunication rb("../../../config/config", id);

	// Start the receiving thread
	std::thread receiveThread(receive, std::ref(rb));
	std::thread printThread(print, std::ref(rb));

	while (g_running) {
		rb.printNodes(&g_lock);
		std::string message = std::string(), idString = std::string();
		std::cout << "Choose which node you want to send the message, or nothing to end the program:" << std::endl;
		std::cin >> idString;
		if (idString.empty()) {
			g_running = false;
			break;
		}
		std::cout << "Write the message:" << std::endl;
		std::cin.ignore(); // Ignore newline left in the buffer
		std::getline(std::cin, message);

		std::vector<unsigned char> messageBytes(message.begin(), message.end());
		rb.send(static_cast<unsigned short>(strtol(idString.c_str(), nullptr, 10)), messageBytes);
	}

	// Notify the receiver thread to stop and wait for it to finish
	if (receiveThread.joinable()) {
		receiveThread.join();
	}

	if (printThread.joinable()) {
		printThread.join();
	}

	return 0;
}
