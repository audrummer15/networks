CC=g++
FLAGS=-lpthread

ftp: ftp_client.cpp ftp_server.cpp
	$(CC) -o client/ftp_client ftp_client.cpp
	$(CC) -o ftp_server ftp_server.cpp $(FLAGS)

ftp_client: ftp_client.cpp
	$(CC) -o client/ftp_client ftp_client.cpp

ftp_server: ftp_server.cpp
	$(CC) -o ftp_server ftp_server.cpp $(FLAGS)

clean:
	rm -rf *.o
