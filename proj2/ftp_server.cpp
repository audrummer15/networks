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

using namespace std;

#define PORT 10035
#define BUFSIZE 512
#define DATASIZE 510
#define HEADERSIZE 2
#define MAXLINE 1024

/* PORTS ASSIGNED TO GROUP
	10034 - 10037 
*/

typedef struct {
	uint8_t Sequence;
	unsigned char Checksum;
	char Data[DATASIZE];
} Packet;

// GLOBAL DATA
struct sockaddr_in myaddr;      		/* our address */
struct sockaddr_in remaddr;     		/* remote address */
socklen_t addrlen = sizeof(remaddr);        /* length of addresses */
int recvlen;                    		/* # bytes received */
int fd;                         		/* our socket */
unsigned char buf[BUFSIZE];    		/* receive buffer */
uint8_t seqnum;
char data[BUFSIZE - HEADERSIZE];

// ACK and NAK constants
char nak[2] = {'0', '0'};
char ack[2] = {'1', '0'};

unsigned char generateChecksum( Packet*); 
void receiveData();
void sendFile(const char*);
bool sendPacket(const Packet*, bool);
Packet* constructPacket(char*, int);

int main()
{    

	/* create a UDP socket */

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket\n");
		return 0;
	}

	/* bind the socket to any valid IP address and a specific port */

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(PORT);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}
	
	cout<<"\n- Reliable FTP Application -\n";
	cout<<"Waiting on port: "<< PORT << "\n";

	/* now loop, receiving data */
	while(1) {
		recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

		if( recvlen == sizeof(ack) ) {
			//ack or nak
		} else if(recvlen == BUFSIZE) {
			Packet* pPacket = new Packet;
			memcpy(pPacket, buf, BUFSIZE);

			//Add data to buffer (minus two byte header)
			for( int x = HEADERSIZE; x < recvlen; x++) {
				data[x - HEADERSIZE] = buf[x];
				cout << buf[x];
			}
			cout << endl;

			string command(data);

			//Make checksum
			if ( generateChecksum(pPacket) != buf[1]) {
				cout << "Checksum invalid - NAK >> " << command << "\n";
				nak[1] = seqnum;
				//sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
			} else {
				if ( command.substr(0,3) == "GET" || command.substr(0,3) == "get" ) {
					cout << "Checksum valid - ACK\n";
					seqnum = pPacket->Sequence;
					ack[1] = seqnum;
					//sendto(fd, ack, 2, 0, (struct sockaddr *)&remaddr, addrlen);
					sendFile(command.substr(4, command.length()).c_str());
					cout << "GET successfully completed" << endl;
				} else {
					cout << "Not a GET command - NAK\n";
					nak[1] = (char)pPacket->Sequence ^ 30;
					//sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
				}
			}

			delete pPacket;
		}
	} /* never exits */
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

void receiveData() {
	ofstream outFile;
	Packet* pPacket = new Packet;
	outFile.open("TestFile.txt",fstream::out | fstream::trunc);
	
	recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
	
	memcpy(pPacket, buf, BUFSIZE);
	
	while ( recvlen > 1 ) {
		//Make checksum
		if ( generateChecksum(pPacket) != buf[1]) {
			cout << "Checksum invalid - NAK - Sequence Num: " << (int)seqnum << "\n";
			nak[1] = seqnum;
			sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
		}
		else {
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
			string buffer(data);

			if ( seqnum != pPacket->Sequence ) {
				seqnum = pPacket->Sequence;
				outFile << buffer;
			}
		}

		recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		memcpy(pPacket, buf, BUFSIZE);
	}

	ack[1] = seqnum;
	sendto(fd, ack, 2, 0, (struct sockaddr *)&remaddr, addrlen);
	outFile.close();
	delete pPacket;
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
		if (sendto(fd, pPacket, BUFSIZE, 0, (struct sockaddr*)&remaddr, addrlen) == -1) {
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

void sendFile(const char* getFile) {
	ifstream iFile;
	iFile.open(getFile);
	unsigned char csum = 0x00;
	unsigned char lost = 0x00;
	char buff[DATASIZE];
	bool bSent = false, bGremlin = false;
	Packet *pPacket = new Packet;
	Packet *pTemp = new Packet;

	if(iFile.is_open())
	{  

		while(!iFile.eof())
		{

			/************************************************/
			// This part of the code reads in from the      //
			// open file and fills up the data part of      //
			// the packet. It also calculates the checksum. //
			/************************************************/
			for(int i = 0; i < DATASIZE; i++)
			{
				if(!iFile.eof())
				{
					buff[i] = iFile.get();
					
					if( i < 48 ) {
						cout << buff[i];
					}
				}
				else
				{
					if( i != 0 ) {
						buff[i-1]= '\0';
					}
					buff[i] = '\0';
				}
			}

			cout << endl;


			/*************************************************/
			// This part of the code looks at the input      //
			// parameters and determines if a packet should  //
			// be simulated lost or damaged. This is also    //
			// known as the GREMLIN function         //
			/*************************************************/

			//Send
			bSent = false;
			pPacket = constructPacket(buff, strlen(buff));
			while( !bSent ) {
				memcpy(pTemp, pPacket, sizeof( Packet ));
				//bGremlin = gremlin(fDamaged, fLost, pTemp);
				bSent = sendPacket(pTemp, bGremlin); 
			}


		}

		iFile.close();
	}
	else
	{
		cout << "Could not open file name.\n";
	}

	sendto(fd, "\0", 1, 0, (struct sockaddr*)&remaddr, addrlen);

	cout << "Sending Complete!\n";
	delete pPacket;
	delete pTemp;
}