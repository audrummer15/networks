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
#define PORT 10035
#define MAXLINE 1024

using namespace std;

void usage();
char generateChecksum(char*, int);
char* loadFileToBuffer();

struct Packet {
    uint8_t Sequence;
    char Checksum;
    char Data[126];
} Packet;

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
    short iSequence = 0;
    bool bSent = false;

    //Other
    char* cFileBuffer = loadFileToBuffer();

    if( cFileBuffer == NULL ) {
        cerr << "Error reading file. Exiting..." << endl;
        return 0;
    }

    cout << cFileBuffer[0] << " " << cFileBuffer[1] << " " << cFileBuffer[2] << endl;

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

    if (sendto(fd, "PUT TestFile", strlen("PUT TestFile"), 0, (struct sockaddr*)&serverAddress, slen) == -1) {
        cerr << "Problem sending data. Exiting..." << endl;
        return 0;
    }

    while( !bSent ) {
        int iLength = 0;
        char recvline[MAXLINE + 1];

        iLength = recvfrom(fd, recvline, MAXLINE, 0, (struct sockaddr*)&serverAddress, &slt);


        bSent = true;
    }

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
          std::cout << "all characters read successfully.";
        else
          std::cout << "error: only " << is.gcount() << " could be read";

        is.close();

        return cBuff;
    }

    return NULL;
}