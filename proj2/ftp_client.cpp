#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring> 
#include <vector>
#include <sstream>
#include <fstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdint.h>

#define BUFSIZE 512
#define DATASIZE 508
#define HEADERSIZE 4
#define PORT 10035
#define MAXLINE 1024
#define WINDOWSIZE 16
#define SEQMODULO 32
#define ACK 1
#define NAK 0

using namespace std;

typedef struct {
	uint8_t Sequence;
	uint8_t Ack;
	uint16_t Checksum;
	char Data[DATASIZE];
} Packet;	

void usage();
uint16_t generateChecksum(char*, int);
Packet* constructPacket(char*, int);
Packet* constructPacket(uint8_t, uint8_t);
bool sendPacket(const Packet*, bool); 
vector<string> getUserInput(void);
bool isCorrupt();
void receiveData(string);

int fd;
int slen;
socklen_t addrlen;
struct sockaddr_in remaddr; 
int recvlen;                    		/* # bytes received */
unsigned char buf[BUFSIZE];    		/* receive buffer */
uint8_t seqnum = 0;
char data[BUFSIZE - HEADERSIZE];
Packet *pPacket;
uint8_t expectedSeq = 0;


int main(int argc, char *argv[])
{
	//Socket variables
	struct hostent *hp;
	unsigned char buf[BUFSIZE];
	slen = sizeof(remaddr);
	addrlen = sizeof(remaddr);

	//Command line arguments
	string sServerAddress;
	float fDamaged = 0;
	float fLost = 0;

	bool bSent = false;
	bool bGremlin = false;
	bool bContinue = true;

	//Sending variable
	Packet *pTemp = new Packet;

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

	//Loop forever
	while(bContinue)
	{
		//Open socket
		if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			cerr << "Unable to open socket. Exiting..." << endl;
			return 0;
		}

		bzero(&remaddr, sizeof(remaddr));
		remaddr.sin_family = AF_INET;
		remaddr.sin_port = htons(PORT);

		if (inet_aton(sServerAddress.c_str(), &remaddr.sin_addr)==0)
		{
			cerr << "There was a problem with the server IP address. Exiting..." << endl;
			return 0;
		}

		cout << "Type command or quit to quit: ";
		
        	vector<string> userInput = getUserInput();

	        if( userInput.front().compare("get") == 0 ) {
	            
			cout << "Getting " << userInput[1] << endl;

			bSent = false;
			userInput.push_back(userInput[1]);
			userInput[1].insert(0, userInput[0] + " ");
			pPacket = constructPacket((char*)userInput[1].c_str(), strlen(userInput[1].c_str()));
			
			/*while( !bSent ) {
				memcpy(pTemp, pPacket, sizeof( Packet ));
				//bGremlin = gremlin(fDamaged, fLost, pTemp);
				cout << "Sending GET..." << endl;
				bSent = sendPacket(pTemp, bGremlin);
			}*/

			cout << "Sending GET..." << endl;
			while( sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr*)&remaddr, slen) == -1);

			receiveData(userInput[2]);
			
			cout << "GET successfully completed\n";

	        } else if( userInput.front().compare("quit") == 0 ) {
			cout << "Good-bye!\n";
			bContinue = false;
	        } else {
			cout << "Unrecognized\n";
	        }

		close(fd);
	}

    	return 0;
}

void usage() {
	cout << "Use the following syntax: \nproject2 -l <lost packets> -d <damaged packets>" << endl;
}

unsigned char generateChecksum( Packet* pPacket ) {
	uint16_t retVal = 0;

	retVal = pPacket->Sequence;

	for( int i=0; i < DATASIZE; i++ ) {
		retVal += pPacket->Data[i];
	}

	retVal = ~retVal;

	return retVal;
}

Packet* constructPacket(char* data, int length) {
	Packet* pPacket = new Packet;
	static uint8_t sequenceNum = 0;

	pPacket->Sequence = sequenceNum;

	sequenceNum = (sequenceNum + 1) % SEQMODULO;

	for( int i=0; i < DATASIZE; i++ ) {
		if( i < length )
			pPacket->Data[i] = data[i];
		else
			pPacket->Data[i] = '\0';
	}

	pPacket->Checksum = generateChecksum(pPacket);

	return pPacket;
}

Packet* constructPacket(uint8_t ak, uint8_t sequenceNum) {
	Packet* pPacket = new Packet;

	pPacket->Sequence = sequenceNum;
	pPacket->Ack = ak;

	for( int i=0; i < DATASIZE; i++ ) {
			pPacket->Data[i] = '\0';
	}

	pPacket->Checksum = generateChecksum(pPacket);

	return pPacket;
}


bool sendPacket(const Packet* pPacket, bool bLost) {
	bool bReturn = false;

	//Send packet
	if(!bLost)
	{
		if (sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr*)&remaddr, addrlen) == -1) {
			cerr << "Problem sending packet with sequence #" << pPacket->Sequence << "..." << endl;
			bReturn = false;
		} else {
			bReturn = true;
		}
	}
  
	return bReturn;
}

vector<string> getUserInput(void) {
	vector<string> result;
	string sLine;

	cout << "> ";

	getline( cin, sLine );

	// Parse words into a vector
	string word;
	istringstream iss(sLine);

	while( iss >> word ) result.push_back(word);

	return result;
}

bool isCorrupt() {
	uint16_t uiChecksum = 0;
	memcpy(&uiChecksum, &buf[2], sizeof(uint16_t));
	return generateChecksum(pPacket) != uiChecksum;
}

void receiveData(string sFilename) {
	ofstream outFile;
	outFile.open(sFilename.c_str(), fstream::out | fstream::trunc);

	recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

	memcpy(pPacket, buf, BUFSIZE);
    
	while ( recvlen > 1 ) {
		//Make checksum
		

		if ( isCorrupt() ) {
			cout << "Checksum invalid - NAK - Sequence Num: " << (int)pPacket->Sequence << "\n";
			pPacket = constructPacket(NAK,expectedSeq);
			sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr *)&remaddr, addrlen);

		} else if( expectedSeq != pPacket->Sequence ) {
			cout << "Out of order packet - ACK - Last Received(Sequence Num): " << (int)(expectedSeq - 1) << "(" << (int)pPacket->Sequence << ")\n";
			pPacket = constructPacket(ACK,expectedSeq - 1);
			sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr *)&remaddr, addrlen);

		} else {
			//Add data to buffer (minus two byte header)
			for( int x = HEADERSIZE; x < recvlen; x++) {
				data[x - HEADERSIZE] = buf[x];
				if ( x < 50 ) {
				    cout << buf[x];
				}
			}

			cout << endl;

			string buffer(data);
			outFile << buffer;

			expectedSeq = (expectedSeq + 1) % SEQMODULO;

			cout << "ACK - Seq Num: " << (int)expectedSeq << "\n";
			pPacket = constructPacket(ACK,expectedSeq);
			sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr *)&remaddr, addrlen);
		}

		recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		memcpy(pPacket, buf, BUFSIZE);
	}

	outFile.close();
	expectedSeq = 0;
}
