#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>

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

const int MAXLINE = 320000;
const int TIMEOUT = 5;

using namespace std;
using namespace std::chrono;

 
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

void printLog(string type, int thissn) {
	cout << timeNow() << ", " << type << ", " << thissn << endl;
}

void dg_receive(int sockfd, sockaddr *pcliaddr, socklen_t clilen, int droppc) {
	int n;
	socklen_t len;
	char outfile_path[MAXLINE];
	char infile_size[MAXLINE];
	char str_mtu[MAXLINE];
	char str_window_size[MAXLINE];
	char str_num_packets[MAXLINE];



	//				Retrieve all of the file information
	// Receive the outfile path
	n = recvfrom(sockfd, outfile_path, MAXLINE, 0, pcliaddr, &len);
	outfile_path[n] = 0;
	cerr << "Outfile path: " << outfile_path << endl;

	// Receive the infile size 
	n = recvfrom(sockfd, infile_size, MAXLINE, 0, pcliaddr, &len);
	infile_size[n] = 0;
	int file_size = strtol(infile_size, NULL, 10);
	cerr << "file size: " << file_size << endl;

	// Receive the MTU 
	n = recvfrom(sockfd, str_mtu, MAXLINE, 0, pcliaddr, &len);
	str_mtu[n] = 0;
	int mtu = strtol(str_mtu, NULL, 10);
	cerr << "MTU: " << mtu << endl;

	// Receive the window size 
	n = recvfrom(sockfd, str_window_size, MAXLINE, 0, pcliaddr, &len);
	str_window_size[n] = 0;
	int window_size = strtol(str_window_size, NULL, 10);
	cerr << "window size: " << window_size<< endl;

	// Receive the total number of packets
	n = recvfrom(sockfd, str_num_packets, MAXLINE, 0, pcliaddr, &len);
	str_num_packets[n] = 0;
	int num_packets = strtol(str_num_packets, NULL, 10);
	cerr << "number of packets: " << num_packets << endl;


	// ACK the above info
	char mesg[12];

	strcpy(mesg, "ACK ");
	len = clilen;
	sendto(sockfd, mesg, n, 0, pcliaddr, len);



	// Use a vector so we dont have to manage our own memory
	vector<string>buffer;
	vector<int>packets_received(num_packets, 0);

	char recvline[mtu+1];
	char sendline[mtu];

	int ack_num = 0;
	int prev_ack_num = 0;
	int thissn = 0;

	struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


	for (int i = 0; i < num_packets; i++) {
		len = clilen;
		memset(&(recvline[0]), 0, mtu+1);



		n = recvfrom(sockfd, recvline, mtu, 0, pcliaddr, &len);
		string line(recvline);
		/// Check for timeout
		if (n < 0) {
			if (errno == EINTR) {
				break;  // waited long enough
			} else if (errno == EWOULDBLOCK) {
				cerr << "Connection timed out" << endl;
				exit(EXIT_FAILURE);	// timed out, do bookkeeping
			} else {
				cerr << "recvfrom error\n";
				exit(EXIT_FAILURE);
			}
		}		


		char *data;
		char *str_ack_num = strtok_r(recvline, ":", &data);

		prev_ack_num = ack_num;
		if (isdigit(str_ack_num[0])) ack_num = stoi(str_ack_num);

		//cerr << "RECEIVED PACKET - " << ack_num << ":" << data << endl;



		if (rand() % 100 < droppc) {
			buffer.insert(buffer.begin() + i, "PACKET_DROPPED");
			packets_received.insert(packets_received.begin() + i, -1);
		}
		else {
			buffer.insert(buffer.begin() + i, data);
			packets_received.insert(packets_received.begin() + i, i);
			printLog("DATA", ack_num);
		}





		// timeout vars
		struct timeval tv;
		tv.tv_sec = TIMEOUT;
		tv.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

		// ack after window number of packets
		if (i > 0 and packets_received.at(i) > -1 and i % window_size == 0 or i == num_packets-1) {
			//send latest ack num
			string ack_packet;
			ack_packet += to_string(ack_num);

			strcpy(sendline, ack_packet.c_str());
			sendto(sockfd, sendline, mtu, 0, pcliaddr, mtu);
			//cout << sendline << endl;

			
			printLog("ACK", ack_num);
		}




	}

	// Open and write to the outfile stream
	fstream outfile;
	outfile.open(outfile_path, ios_base::out | ios::binary);

	
	for (int i = 0; i < buffer.size(); i++) {
		outfile << buffer.at(i);
	}
	outfile.close();

}


int main(int argc, char **argv){
	int sockfd;
	struct sockaddr_in servaddr, cliaddr;

	int serv_port = atoi(argv[1]);
	int droppc = atoi(argv[2]);
	char *outfile_path = (argv[3]);


	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(serv_port);

	bind(sockfd, (sockaddr *) &servaddr, sizeof(servaddr));

	dg_receive(sockfd, (sockaddr *) &cliaddr, sizeof(cliaddr), droppc);

}

