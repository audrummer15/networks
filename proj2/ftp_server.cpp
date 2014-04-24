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
#include <sys/stat.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

#define PORT 10035
#define TIMEOUT 20000 //20 ms or 20000 us
#define BUFSIZE 512
#define DATASIZE 508
#define HEADERSIZE 4
#define MAXLINE 1024
#define WINDOWSIZE 16
#define SEQMODULO 32
#define ACK 1
#define NAK 0
#define NORMAL_PACKET 0
#define DELAYED_PACKET 1
#define LOST_PACKET 2

/* PORTS ASSIGNED TO GROUP
	10034 - 10037 
*/

typedef struct {
	uint8_t Sequence;
	uint8_t Ack;
	uint16_t Checksum;
	char Data[DATASIZE];
} Packet;

// GLOBAL DATA
struct timeval sendTime[SEQMODULO];
struct timeval currentTime;
pthread_t sendThread[SEQMODULO];
struct sockaddr_in myaddr;      		/* our address */
struct sockaddr_in remaddr;     		/* remote address */
socklen_t addrlen = sizeof(remaddr);        /* length of addresses */
int recvlen;                    		/* # bytes received */
int fd;                         		/* our socket */
unsigned char buf[BUFSIZE];    		/* receive buffer */
uint8_t seqnum;
char data[BUFSIZE - HEADERSIZE];
uint8_t nextSequenceNum = 1;
long lTotalSegments = 0;
long lBase = 1;

uint16_t generateChecksum( Packet*); 
void sendFile(const char*);
void sendPacket(const Packet*, bool);
Packet* constructPacket(char*, int);
long GetFileSize(std::string);
bool notcorrupt(Packet*);
int gremlin(float, float, float, Packet*);
float fDamaged = 0;
float fLost = 0; 
float fDelayed = 0;
int min(int a, int b);
long lDelay;
long ackCount = 0;


int main(int argc, char *argv[])
{    
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
				fDelayed = atof(argv[i+1]);
			};
			break;

			case 't':
			{
				lDelay = atol(argv[i+1]);
			};
			break;
			
			case 'h':
			{
				return 0;
			}
		}
	}
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

		if(recvlen == BUFSIZE) {
			Packet* pPacket = new Packet;
			memcpy(pPacket, buf, BUFSIZE);

			//Add data to buffer (minus header)
			for( int x = HEADERSIZE; x < recvlen; x++) {
				data[x - HEADERSIZE] = buf[x];
				cout << buf[x];
			}
			cout << endl;

			string command(pPacket->Data); //AMB

			if ( notcorrupt(pPacket) ) {
				if ( command.substr(0,3) == "GET" || command.substr(0,3) == "get" ) {
					cout << "Checksum valid GET - ACK\n";
					seqnum = pPacket->Sequence;
					//ack[1] = seqnum;
					//sendto(fd, ack, 2, 0, (struct sockaddr *)&remaddr, addrlen);
					if(GetFileSize( command.substr(4, command.length()).c_str() ) % DATASIZE == 0)
					{
						lTotalSegments = GetFileSize( command.substr(4, command.length()).c_str() ) / DATASIZE;
					}
					else
					{
						lTotalSegments = GetFileSize( command.substr(4, command.length()).c_str() ) / DATASIZE + 1;
					}

					sendFile(command.substr(4, command.length()).c_str());
					cout << "GET successfully completed" << endl;
				} else {
					//cout << "Not a GET command - NAK (" << command.substr(0,3) << ")\n";
					//nak[1] = (char)pPacket->Sequence ^ 30;
					//sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
				}
			} else {
				//cout << "landed invalid" << endl;
				//cout << "Checksum invalid - NAK >> " << command << "\n";
				//nak[1] = seqnum;
				//sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
			}

			delete pPacket;
		}
	} /* never exits */
}


uint16_t generateChecksum( Packet* pPacket ) {
	uint16_t retVal = 0;

	retVal = pPacket->Sequence;

	for( int i=0; i < DATASIZE; i++ ) {
		retVal += pPacket->Data[i];
	}

	retVal = ~retVal;

	return retVal;
}

void constructPacket(char* data, int length, Packet* pPacket) {
	//Packet* pPacket = new Packet;
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

	return;
}

void *sendPacket(void *args) {

	//Call the gremlin function
	Packet * tbSent = (Packet *)args;
	//cout << "Sent Packet Number: " << (int)tbSent->Sequence << endl;
	int gremlinReturn = gremlin(fDamaged, fLost, fDelayed, tbSent);
	if(gremlinReturn == LOST_PACKET)
	{
	}
	else if(gremlinReturn == DELAYED_PACKET)
	{
		cout << "Delaying - (" << (int)tbSent->Sequence << ")" << endl;
		usleep(1000 * lDelay);
		cout << "Done Delaying - (" << (int)tbSent->Sequence << ")" << endl;
		while(sendto(fd, tbSent, BUFSIZE, 0, (struct sockaddr*)&remaddr, addrlen) == -1)
		{	
			cerr << "Problem sending packet with sequence #" << tbSent->Sequence << "..." << endl; //Keep trying to send
		}

		cout << "Sent: (" << (int)tbSent->Sequence << "): ";

		for( int i=0; i<48; i++ ) 
			cout << tbSent->Data[i];

		cout << endl;
	}
	else
	{
		while(sendto(fd, tbSent, BUFSIZE, 0, (struct sockaddr*)&remaddr, addrlen) == -1)
		{	
			cerr << "Problem sending packet with sequence #" << tbSent->Sequence << "..." << endl; //Keep trying to send
		}

		cout << "Sent: (" << (int)tbSent->Sequence << "): ";

		for( int i=0; i<48; i++ ) 
			cout << tbSent->Data[i];

		cout << endl;
	}

}

void sendFile(const char* getFile) {
	ifstream iFile;
	iFile.open(getFile);
	unsigned char csum = 0x00;
	unsigned char lost = 0x00;
	char buff[DATASIZE];
	bool bSent = false;
	bool bGremlin = false;
	Packet *pPackets[lTotalSegments];
	Packet *pTemps[lTotalSegments];

	//Initialize packet set
	for( int i=0; i < lTotalSegments; i++ ) {
		pPackets[i] = new Packet;
		pTemps[i] = new Packet;
	}

	if(iFile.is_open())
	{  
		int k = 0;

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

				constructPacket(buff, strlen(buff), pPackets[k]);
				k++;


		}

		iFile.close();

		////////////////////////////////////////
		//Send the first window full of stuff //
		////////////////////////////////////////
		
		lBase = 1;		
		nextSequenceNum = 1;
		//cout << "lTotalSeg: " << (int)lTotalSegments << endl;
		for(int k=0; k < min(WINDOWSIZE, lTotalSegments); k++) {
			memcpy(pTemps[k], pPackets[k], sizeof( Packet ));
			gettimeofday(&sendTime[k], NULL); //Gets the current time
			//cout << "Sent a packet..." << (int)pTemps[k]->Sequence << endl;
			pthread_create(&sendThread[k], NULL, sendPacket, pTemps[k]);
		}
Packet *pTemp = new Packet;
		while(ackCount < lTotalSegments)
		{
			//Start Poll on base
			int recvPollVal = 0;
			int iLength = 0;
			struct pollfd ufds;
			unsigned char recvline[MAXLINE + 1];
			

			ufds.fd = fd;
			ufds.events = POLLIN;
			gettimeofday(&currentTime, NULL);
			recvPollVal = poll(&ufds, 1, (TIMEOUT - (currentTime.tv_usec - sendTime[lBase % SEQMODULO].tv_usec)) / 1000);

			if( recvPollVal == -1 ) {
				cerr << "Error polling socket..." << endl;
			} else if( recvPollVal == 0 ) { //We timed out right here
				//cerr << "Timeout... Lost Packet, Sequence Number - " << lBase << endl;
				
				//Send everything again
				for(int k = lBase - 1; k < min(lBase + WINDOWSIZE, lTotalSegments); k++) {
					memcpy(pTemps[k], pPackets[k], sizeof( Packet ));
					gettimeofday(&sendTime[k % SEQMODULO], NULL); //Gets the current time
			
					cout << "Timeout On Packet " << (int)pPackets[k]->Sequence << endl;
					pthread_create(&sendThread[k % SEQMODULO], NULL, sendPacket, pTemps[k]);
				}
				
			} else {
				
				iLength = recvfrom(fd, recvline, MAXLINE, 0, (struct sockaddr*)&remaddr, &addrlen);
				memcpy(pTemp, recvline, BUFSIZE);

				if( pTemp->Ack == ACK ) {
					if((pTemp->Sequence == lBase % SEQMODULO)) {
						ackCount++;
						cout << "ACK - " << (int)pTemp->Sequence << endl;
						if(lBase + WINDOWSIZE < lTotalSegments) {
							memcpy(pTemps[lBase + WINDOWSIZE - 1], pPackets[lBase + WINDOWSIZE - 1], sizeof( Packet ));
							gettimeofday(&sendTime[(lBase + WINDOWSIZE - 1) % SEQMODULO], NULL); //Gets the current time
							pthread_create(&sendThread[(lBase + WINDOWSIZE - 1) % SEQMODULO], NULL, sendPacket, pTemps[lBase]);
							cout << "Sent a packet..." << (int)pPackets[lBase+WINDOWSIZE - 1]->Sequence << endl;
						}
						lBase++;
					}
				} else {

					//NAK, Send everything again
					if((pTemp->Sequence == lBase % SEQMODULO)) {
						for(int k = lBase - 1; k < min(lBase + WINDOWSIZE, lTotalSegments); k++) {
							memcpy(pTemps[k], pPackets[k], sizeof( Packet ));
							gettimeofday(&sendTime[k % SEQMODULO], NULL); //Gets the current time

							cout << "NAK - " << (int)pTemp->Sequence << endl;
							pthread_create(&sendThread[k % SEQMODULO], NULL, sendPacket, pTemps[k]);
						}
					}

				}
				//if ack
				//if nak
			}


		}
			delete pTemp;
		
	}
	else
	{
		cout << "Could not open file name.\n";
	}

	for(int i=0; i < SEQMODULO; i++) {
		if( sendThread[i] != NULL ) {
			pthread_join(sendThread[i], NULL);
		}
	}

	sendto(fd, "\0", 1, 0, (struct sockaddr*)&remaddr, addrlen);

	cout << "Sending Complete!\n";

	lBase = 1;
	nextSequenceNum = 1;
	lTotalSegments = 0;

	//Free up memory
	for(int i=0; i < lTotalSegments; i++) {
		delete pPackets[i];
		delete pTemps[i];
	}
}

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

bool notcorrupt(Packet* pPacket) {
	bool bReturn = false;
	uint16_t uiChecksum = 0;

	//Make checksum	
	memcpy( &uiChecksum, &buf[2], sizeof( uint16_t ) );

	if ( generateChecksum(pPacket) != uiChecksum) {
		bReturn = false;
	} else {
		bReturn = true;
	}

	return bReturn;
}

int gremlin(float fDamaged, float fLost, float fDelayed, Packet* ppacket)
{
	if(fLost > (1.0 * rand()) / (1.0 * RAND_MAX))
	{
		return LOST_PACKET;
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
		return NORMAL_PACKET;

	}
	else if(fDelayed > (1.0 * rand()) / (1.0 * RAND_MAX))
	{
		return DELAYED_PACKET;
	}
	return NORMAL_PACKET;
}

int min(int a, int b)
{
	if(a < b)
	{
		return a;
	}
	return b;
}
