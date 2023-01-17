#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include "servers.h"

using namespace std;
using namespace std::chrono;

int HEADER = 10;
time_t TIMEOUT = 5;

// Fill a vector of Server objects with their respective IP and PORT
void parseConfig(string conf_file_name, vector<Server> &servers) {

	ifstream conf_file;
	conf_file.open(conf_file_name);

	string server_info;
	while (getline(conf_file, server_info)) {

		string temp_ip; 
		char *ip;
		int temp_port;
		int port;

		if (server_info[0] == '#') continue;
		
		stringstream ss(server_info);
		ss >> temp_ip >> temp_port;

		Server s = Server(temp_ip, temp_port);
		servers.push_back(s);
	}
	for (int i = 0; i < 3; i++) {
		cout << servers.at(i).ip << ", " << servers.at(i).port << endl;
	}

	conf_file.close();
}

string timeNow() {
	const auto now_ms = time_point_cast<milliseconds>(system_clock::now());
	const auto now_s = time_point_cast<seconds>(now_ms);
	const auto millis = now_ms - now_s;
	const auto c_now = system_clock::to_time_t(now_s);

	stringstream ss;
	ss << put_time(gmtime(&c_now), "%FT%T")
		<< '.' << setfill('0') << setw(3) << millis.count() << 'Z';
	return ss.str();
}

void printLog(int lport, string rip, int rport, string type, int thissn, int basesn, int nextsn, int winsz) {
	cout << timeNow() << ", " << lport << ", " << rip << ", " << rport;
	cout << ", " << type << ", " << thissn << ", " << basesn << ", " << nextsn;
	cout << ", " << basesn+winsz << endl;
}

int getFileSize(char *infile_name) {

	//auto file_size = filesystem::file_size(infile_name);
	ifstream in_file(infile_name, ios::binary);
	in_file.seekg(0, ios::end);
	int file_size = in_file.tellg();

	return file_size;
}

/*
 *	dgCli
 *    a. connect the socket
 		b. in a while loop:
		 i. write to the socket from an infile filestream
		 ii. check for errors, timeouts
		 iii. read the contents of the received line in the socket to the outfile
		c. create an outstream to a file "output.dat"
		d. close the outfile
 */
void dgCli(vector<Server>servers, int sockfd, const sockaddr *pservaddr,
		socklen_t servlen, int mtu, int window_size, char *infile_name, char *outfile_name) {

	int n;
	int payload_size = mtu - HEADER;
	char sendline[mtu], recvline[mtu];
	int	thissn = 0, basesn = 0, nextsn = 0;
	int lport = servers.at(0).port;
	int rport = servers.at(0).port;
	const char *rip = servers.at(0).ip.c_str();

	fstream infile;
	infile.open(infile_name, std::ios_base::in | std::ios::binary);


	int retval = connect(sockfd, (sockaddr *)pservaddr, servlen);

	if (retval < 0) {
		cerr << "Cannot detect server\n";
		exit(EXIT_SUCCESS);
	}

	socklen_t len;
	sockaddr *preply_addr;
	preply_addr = (sockaddr *)malloc(servlen);

	// Variables for packet error checking
	struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	// Send over the desired outfile path
	strcpy(sendline, outfile_name);
	sendline[strlen(sendline)] = 0;
	write(sockfd, sendline, strlen(sendline));

	// Send over infile size
	//int file_size = filesystem::file_size(infile_name);
	int file_size = getFileSize(infile_name);
	char infile_size[12];
	//cerr << "file size: " << file_size << endl;
	sprintf(infile_size, "%d", file_size);
	strcpy(sendline, infile_size);
	sendline[strlen(infile_size)] = '\0';
	write(sockfd, sendline, strlen(sendline));

	// Send over the MTU
	char str_mtu[12];
	sprintf(str_mtu, "%d", mtu);
	strcpy(sendline, str_mtu);
	sendline[strlen(str_mtu)] = '\0';
	write(sockfd, sendline, strlen(sendline));

	// Send over the window size
	char str_window_size[12];
	sprintf(str_window_size, "%d", window_size);
	strcpy(sendline, str_window_size);
	sendline[strlen(str_window_size)] = '\0';
	write(sockfd, sendline, strlen(sendline));

	// Calculate and send over the number of packets
	int num_packets = (file_size + payload_size - 1) / payload_size;
	char str_num_packets[12];
	sprintf(str_num_packets, "%d", num_packets);
	strcpy(sendline, str_num_packets);
	sendline[strlen(str_num_packets)] = '\0';
	write(sockfd, sendline, strlen(sendline));

	// Wait to see if the server is up and has received the above info
	n = read(sockfd, recvline, mtu);
	// Checking for errors, timeouts
	if (n < 0) {
		if (errno == EINTR) {
			;  // waited long enough
		} else if (errno == EWOULDBLOCK) {
			cerr << "Connection timed out" << endl;
			exit(EXIT_FAILURE);
		} else {
			cerr << "Cannot detect server\n";
			exit(EXIT_FAILURE);
		}
	}		



	// Load the file into a buffer
	vector<string>file_buffer;

	for (int i = 0; i < num_packets; i++) {

		string packet;
		string data(payload_size, 0);

		packet += to_string(i);
		packet += ':';

		infile.read(&data[0], payload_size);
		packet += data;
		
		//cerr << packet << endl;

		file_buffer.push_back(packet);
		
	}
	
	int i = 0;
	while (i < num_packets) {

		memset(&(sendline[0]), 0, mtu);

		strcpy(sendline, file_buffer.at(i).c_str());

		//cerr <<"SENDLINE: "<< sendline << endl;

		write(sockfd, sendline, mtu);

		thissn = i;
		printLog(lport, rip, rport, "DATA", thissn, basesn, nextsn, window_size);

		len = servlen;



		if (len != servlen) {
			cerr << "Packet loss detected\n";
			continue;
		}

		if (i > 0 and i+1 % window_size == 0 or i == num_packets-1) {
			n = read(sockfd, recvline, mtu);
			printLog(lport, rip, rport, "ACK", thissn, basesn, nextsn, window_size);
			// Checking for errors, timeouts
			if (n < 0) {
				if (errno == EINTR) {
					break;  // waited long enough
				} else if (errno == EWOULDBLOCK) {
					continue;
					cerr << "Connection timed out" << endl;
					exit(EXIT_FAILURE);
				} else {
					break;
				}
			}	


		}
		
		i++;
	}

}

int main(int argc, char **argv) {

	if (argc < 6) {
		cerr << "Insufficient arguments supplied." << endl;
		return EXIT_FAILURE;
	}

	
	// parse, declare, and initialize socket vars
	int num_servers = stoi(argv[1]);
	string conf_file_name = argv[2];
	int mtu = atoi(argv[3]);
	int window_size = atoi(argv[4]);

	if (mtu <= 10) {
		cerr << "Required minimum MTU is " << HEADER+1 << endl;
		exit(EXIT_FAILURE);
	}

	// parse file names
	char *infile_name = argv[5];
	char *outfile_name = argv[6];

	// Fill a array of servers containing their IP and PORT
	vector<Server>servers;
	parseConfig(conf_file_name, servers);



	const char *rip = servers.at(0).ip.c_str();
	int rport= servers.at(0).port;

	int sockfd;
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(rport);


	// create the socket
	if (inet_pton(AF_INET, rip, &servaddr.sin_addr) <= 0) {
		cout << rip << endl;
		cerr << "Invalid IP address.\n";
		exit(EXIT_FAILURE);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		cerr << "Socket error.\n";
		exit(EXIT_SUCCESS);
	}

	// create infile stream
	//FILE *infile_ptr;
	//infile_ptr = fopen(infile_name, "r");


	dgCli(servers, sockfd, (sockaddr *) &servaddr, sizeof(servaddr), 
			mtu, window_size, infile_name, outfile_name);

	exit(EXIT_SUCCESS);
}

