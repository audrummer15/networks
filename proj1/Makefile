CC=g++
FLAGS=-lm

ftp: ftp_client.cpp ftp_server.cpp
	$(CC) -o ftp_client ftp_client.cpp
	$(CC) -o ftp_server ftp_server.cpp

ftp_client: ftp_client.cpp
	$(CC) -o ftp_client ftp_client.cpp

ftp_server: ftp_server.cpp
	$(CC) -o ftp_server ftp_server.cpp

clean:
	rm -rf *.o
