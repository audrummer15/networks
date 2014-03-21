#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring> 
#include <vector>
#include <sstream>
#include <fstream>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdint.h>

#define BUFSIZE 128
#define DATASIZE 126
#define HEADERSIZE 2
#define PORT 10035
#define MAXLINE 1024

using namespace std;

typedef struct {
    uint8_t Sequence;
    char Checksum;
    char Data[DATASIZE];
} Packet;	

void usage();
char generateChecksum(char*, int);
char* loadFileToBuffer();
Packet* constructPacket(char*, int);

int main(int argc, char *argv[])
{
    //Socket variables
    struct hostent *hp;
    struct sockaddr_in serverAddress; 
    unsigned char buf[BUFSIZE];
    int fd;
    int slen=sizeof(serverAddress);
    socklen_t slt = sizeof(serverAddress);

    //Command line arguments
    string sServerAddress;
    float fDamaged = 0;
    float fLost = 0;

    //Sending variables
    uint8_t iSequence = 0;
    Packet *pPacket;
    bool bSent = false; //File has successfully sent

    //Other
    char* cFileBuffer = loadFileToBuffer();

    if( cFileBuffer == NULL ) {
        cerr << "Error reading file. Exiting..." << endl;
        return 0;
    }

    for(int i=1;i < argc; i+= 2)
    {
        switch (argv[i] [1])
        {
            case 'd':
            {
                fDamaged = atof(argv[i+1]);
            };
            break;

            case 'l':
            {
                fLost = atof(argv[i+1]);
            };
            break;

            case 's':
            {
                sServerAddress = argv[i+1];
            };
            break;

            case 'h':
            {
                usage();
                return 0;
            }
        }
    }

    //Open socket
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cerr << "Unable to open socket. Exiting..." << endl;
        return 0;
    }

    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);

    if (inet_aton(sServerAddress.c_str(), &serverAddress.sin_addr)==0)
    {
        cerr << "There was a problem with the server IP address. Exiting..." << endl;
        return 0;
    }

    pPacket = constructPacket((char*)"zzzzzzzzzzzzzzzz", strlen("zzzzzzzzzzzzzzzz"));

    if (sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr*)&serverAddress, slen) == -1) {
        cerr << "Problem sending data. Exiting..." << endl;
        return 0;
    }

    while( !bSent ) {
        int iLength = 0;
        char recvline[MAXLINE + 1];

        iLength = recvfrom(fd, recvline, MAXLINE, 0, (struct sockaddr*)&serverAddress, &slt);


        bSent = true;
    }

	delete pPacket;
    return 0;
}

void usage() {
    cout << "Use the following syntax: \nproject1 -l <lost packets> -d <damaged packets>" << endl;
}

char generateChecksum( char* data, int length ) {
    char retVal = 0x00;

    for( int i=0; i < length; i++ ) {
        retVal += *(data + i);
    }

    retVal = ~retVal;

    return retVal;
}

char* loadFileToBuffer() {
    std::ifstream is ("TestFile", std::ifstream::binary);

    if (is) {
        // get length of file:
        is.seekg (0, is.end);
        int length = is.tellg();
        is.seekg (0, is.beg);

        char* cBuff = new char[length];

        std::cout << "Reading " << length << " characters... ";
        // read data as a block:
        is.read (cBuff,length);

        if (is)
          std::cout << "all characters read successfully.\n";
        else
          std::cout << "error: only " << is.gcount() << " could be read\n";

        is.close();

        return cBuff;
    }

    return NULL;
}

Packet* constructPacket(char* data, int length) {
	Packet* pPacket = new Packet;
	static uint8_t sequenceNum = 0;

	pPacket->Sequence = sequenceNum;

	sequenceNum = 1 - sequenceNum;

	for( int i=0; i < DATASIZE; i++ ) {
		if( i < length )
			pPacket->Data[i] = data[i];
		else
			pPacket->Data[i] = '\0';
	}

	pPacket->Checksum = generateChecksum((char*)pPacket->Data, DATASIZE);

	return pPacket;
}

