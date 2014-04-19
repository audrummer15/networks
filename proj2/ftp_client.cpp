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
#define DATASIZE 510
#define HEADERSIZE 2
#define PORT 10035
#define MAXLINE 1024

using namespace std;

typedef struct {
	uint8_t Sequence;
	unsigned char Checksum;
	char Data[DATASIZE];
} Packet;	

void usage();
char generateChecksum(char*, int);
bool gremlin(float, float, Packet*);
char* loadFileToBuffer();
Packet* constructPacket(char*, int);
bool sendPacket(const Packet*, bool); 
vector<string> getUserInput(void);
void receiveData(string);

int fd;
int slen;
socklen_t addrlen;
struct sockaddr_in remaddr; 
int recvlen;                    		/* # bytes received */
unsigned char buf[BUFSIZE];    		/* receive buffer */
uint8_t seqnum = 1;
char data[BUFSIZE - HEADERSIZE];
Packet *pPacket;

// ACK and NAK constants
char nak[2] = {'0', '0'};
char ack[2] = {'1', '0'};

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
			
			cout << "Sending Complete!\n";

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

bool gremlin(float fDamaged, float fLost, Packet* ppacket)
{
	if(fLost > (1.0 * rand()) / (1.0 * RAND_MAX))
	{
		return true;
	}

	if(fDamaged > (1.0 * rand()) / (1.0 * RAND_MAX))
	{
		int numDamaged = rand() % 10;
		int byteNum = rand() % BUFSIZE;
		if(numDamaged == 9)
		{
			if(numDamaged > 1)
			{
				ppacket->Data[numDamaged-HEADERSIZE]+= 8;	
			}
			else if(numDamaged == 1)
			{
				ppacket->Checksum+= 8;
			}
			else
			{
				ppacket->Sequence+= 8;
			}
			
			numDamaged = rand() % 10;
			if(numDamaged > 1)
			{
				ppacket->Data[numDamaged-HEADERSIZE]+= 4;	
			}
			else if(numDamaged == 1)
			{
				ppacket->Checksum+= 4;
			}
			else
			{
				ppacket->Sequence+= 4;
			}

			numDamaged = rand() % 10;
			if(numDamaged > 1)
			{
				ppacket->Data[numDamaged-HEADERSIZE]+= 2;	
			}
			else if(numDamaged == 1)
			{
				ppacket->Checksum+= 2;
			}
			else
			{
				ppacket->Sequence+= 2;
			}
		}
		else if(numDamaged > 6)
		{
			if(numDamaged > 1)
			{
				ppacket->Data[numDamaged-HEADERSIZE]+= 8;	
			}
			else if(numDamaged == 1)
			{
				ppacket->Checksum+= 8;
			}
			else
			{
				ppacket->Sequence+= 8;
			}

			numDamaged = rand() % 10;
			if(numDamaged > 1)
			{
				ppacket->Data[numDamaged-HEADERSIZE]+= 4;	
			}
			else if(numDamaged == 1)
			{
				ppacket->Checksum+= 4;
			}
			else
			{
				ppacket->Sequence+= 4;
			}
		}
		else
		{
			if(numDamaged > 1)
			{
				ppacket->Data[numDamaged-HEADERSIZE]+=8;	
			}
			else if(numDamaged == 1)
			{
				ppacket->Checksum+=4;
			}
			else
			{
				ppacket->Sequence+= 2;
			}
		}

	}
	return false;
}

void usage() {
	cout << "Use the following syntax: \nproject1 -l <lost packets> -d <damaged packets>" << endl;
}

unsigned char generateChecksum( Packet* pPacket ) {
	unsigned char retVal = 0x00;

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

	sequenceNum = 1 - sequenceNum;

	cout << "Seq: " << (int)sequenceNum << endl;

	for( int i=0; i < DATASIZE; i++ ) {
		if( i < length )
			pPacket->Data[i] = data[i];
		else
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
		if (sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr*)&remaddr, slen) == -1) {
			cerr << "Problem sending packet with sequence #" << pPacket->Sequence << "..." << endl;
			bReturn = false;
		} 
	}


	int recvPollVal = 0;
	int iLength = 0;
	struct pollfd ufds;
	unsigned char recvline[MAXLINE + 1];
	time_t timer;

	//Wait on return
	ufds.fd = fd;
	ufds.events = POLLIN;
	recvPollVal = poll(&ufds, 1, 20);

	if( recvPollVal == -1 ) {            //If error occurs
		cerr << "Error polling socket..." << endl;
		bReturn = false;
	} else if( recvPollVal == 0 ) {        //If timeout occurs
		cerr << "Timeout... Lost Packet, Sequence Number - "  << (int)pPacket->Sequence << endl;
		bReturn = false;
	} else {
		iLength = recvfrom(fd, recvline, MAXLINE, 0, (struct sockaddr*)&remaddr, &addrlen);

		if( recvline[0] == '1') {          //If ACK Received, return true
			cout << "ACK - " << (int)recvline[1] << endl;
			//cout << pPacket->Data << endl;
			bReturn = true;
		} else if( recvline[0] == '0' ) {     //Else if NAK, return false
			cout << "NAK - " << (int)recvline[1] << endl;
			bReturn = false;
		} else {                             //Else bad stuff, return false
			bReturn = false;
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

void receiveData(string sFilename) {
    	ofstream outFile;
    	outFile.open(sFilename.c_str(), fstream::out | fstream::trunc);

    	recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

    	memcpy(pPacket, buf, BUFSIZE);
    
	while ( recvlen > 1 ) {
		//Make checksum
		if ( generateChecksum(pPacket) != buf[1]) {
			cout << "Checksum invalid - NAK - Sequence Num: " << (int)seqnum << "\n";
			nak[1] = seqnum;
			sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
		} else {
			cout << "ACK - Seq Num: " << (int)seqnum << "\n";
			ack[1] = seqnum;
			sendto(fd, ack, 2, 0, (struct sockaddr *)&remaddr, addrlen);
			
			//Add data to buffer (minus two byte header)
			for( int x = HEADERSIZE; x < recvlen; x++) {
				data[x - HEADERSIZE] = buf[x];
				if ( x < 50 ) {
				    cout << buf[x];
				}
			}

			cout << endl;
			
			//If the sequence number changed (not duplicate)
			if ( seqnum != pPacket->Sequence ) {
				string buffer(data);
				seqnum = pPacket->Sequence;
				outFile << buffer;
			}
			else
				cout << "This is why..." << endl;
		}

		recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		memcpy(pPacket, buf, BUFSIZE);
	}


	ack[1] = seqnum;
	sendto(fd, ack, 2, 0, (struct sockaddr *)&remaddr, addrlen);
	outFile.close();
}
