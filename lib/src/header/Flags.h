
#ifndef FLAGS_H
#define FLAGS_H
struct Flags {
	bool ACK = false;
	bool SYN = false;
	bool FIN = false;
	bool END = false;
	bool BROADCAST = false;
	bool HEARTBEAT = false;
	bool JOIN = false;
	bool SYNCHRONIZE = false;
};

#endif // FLAGS_H
