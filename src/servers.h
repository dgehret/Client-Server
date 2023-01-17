
//#pragma once
#include <string>
using namespace std;

class Server {
	public:
		string ip;
		int port;

		Server() {}

		Server(string ip, int port) {
			this->ip = ip;
			this->port = port;
		}
};

