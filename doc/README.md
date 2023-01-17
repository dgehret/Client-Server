I - Useage
To use this application, run make and enter your commands in the following format:

(if in the top directory)
bin/myserver port_number droppc

			--AND (in a different terminal)--

bin/myclient serve_rip server_port mtu winsz in_file_path out_file_path





II - Internal Design
The internal design is as follows:

0. Overhead/libraries

1. Parsing command line arguments, error checking

2. Declaring, initializing, and setting socket related variables

3. Calling the function dg_cli to send and receive the file:
	a. connecting the socket
	b. in a while loop:
		i. writing to the socket from an infile filestream
		ii. checking for errors, timeouts
		iii. reading the contents of the received line in the socket to the outfile
	c. creating an outstream to a file "output.dat"
	d. closing the outfile



III - Testing


bin/myserver 9090 0

bin/myclient 3 servaddr.conf 15 10 doc/README outfile


bin/myserver 9090 100

bin/myclient 3 servaddr.conf 512 10 doc/Documentation outfile


bin/myserver 9090 80

bin/myclient 3 servaddr.conf 1500 10 Makefile outfile


bin/myserver 9090 1

bin/myclient 3 servaddr.conf 11 10 Makefile outfile


bin/myserver 9090 20

bin/myclient 3 servaddr.conf 200 10 src/client.cc outfile

