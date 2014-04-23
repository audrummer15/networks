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

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdint.h>

using namespace std;

#define PORT 10035
#define BUFSIZE 512
#define DATASIZE 510
#define HEADERSIZE 2
#define MAXLINE 1024
#define WINDOWSIZE 16
#define SEQMODULO 32

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
uint8_t lastACK = 0;
uint8_t sequenceNum = 0;
uint8_t nextSequenceNum = 1;
long lTotalSegments = 0;
long lBase = 1;


unsigned char generateChecksum( Packet*); 
void sendFile(const char*);
void segmentFile(const char*);
bool sendPacket(const Packet*, bool);
Packet* constructPacket(char*, int);
long GetFileSize(std::string);

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

		if(recvlen == BUFSIZE) {
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
				//cout << "Checksum invalid - NAK >> " << command << "\n";
				//nak[1] = seqnum;
				//sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
			} else {
				if ( command.substr(0,3) == "GET" || command.substr(0,3) == "get" ) {
					//cout << "Checksum valid - ACK\n";
					seqnum = pPacket->Sequence;
					//ack[1] = seqnum;
					//sendto(fd, ack, 2, 0, (struct sockaddr *)&remaddr, addrlen);
					lTotalSegments = GetFileSize( command.substr(4, command.length()).c_str() ) / DATASIZE;
					//segmentFile(command.substr(4, command.length()).c_str());
					cout << "Segmented" << endl;
					sendFile(command.substr(4, command.length()).c_str());
					cout << "GET successfully completed" << endl;
				} else {
					//cout << "Not a GET command - NAK\n";
					//nak[1] = (char)pPacket->Sequence ^ 30;
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

Packet* constructPacket(char* data, int length) {
	Packet* pPacket = new Packet;
	//static uint8_t sequenceNum = 0; moved to global AMB

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

void sendFile(const char* getFile) {
	ifstream iFile;
	iFile.open(getFile);
	unsigned char csum = 0x00;
	unsigned char lost = 0x00;
	char buff[DATASIZE];
	bool bSent = false, bGremlin = false;
	Packet *pPackets[lTotalSegments];
	Packet *pTemps[lTotalSegments];
	bool bEOF = false;
	int iLastPacket = lTotalSegments - 1;

	//Initialize packet set
	for( int i=0; i < lTotalSegments; i++ ) {
		pPackets[i] = new Packet;
		pTemps[i] = new Packet;
	}

	if(iFile.is_open())
	{  

		while(!iFile.eof())
		{

			for( int k=0; k < lTotalSegments; k++ ) {

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
						bEOF = true;
					}
				}

				cout << endl;


				/*************************************************/
				// This part of the code looks at the input      //
				// parameters and determines if a packet should  //
				// be simulated lost or damaged. This is also    //
				// known as the GREMLIN function        	 //
				/*************************************************/

				//Send
				bSent = false;
				pPackets[k] = constructPacket(buff, strlen(buff));

				if( bEOF ) {
					//Mark the last packet in window size to send so we aren't sending empty packets
					// in the next loop.
					iLastPacket = k;
					break;
				}
			}

			//We need to be sending these 16 packets at the same time listening for ack/nacks?? Threads??
			// AMB
			for(int k=0; k <= iLastPacket; k++) {
				while( !bSent ) {
					memcpy(pTemps[k], pPackets[k], sizeof( Packet ));
					//bGremlin = gremlin(fDamaged, fLost, pTemp);
					bSent = sendPacket(pTemps[k], bGremlin); 
				}
				bSent = false;
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

	sequenceNum = 0;

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

void segmentFile(const char* getFile) {
	ifstream iFile;
	iFile.open(getFile);
	unsigned char csum = 0x00;
	unsigned char lost = 0x00;
	char buff[DATASIZE];
	bool bSent = false, bGremlin = false;
	Packet *pPackets[lTotalSegments];
	Packet *pTemps[lTotalSegments];
	bool bEOF = false;
	int iLastPacket = lTotalSegments - 1;

	//Initialize packet set
	for( int i=0; i < lTotalSegments; i++ ) {
		pPackets[i] = new Packet;
		pTemps[i] = new Packet;
	}

	if(iFile.is_open())
	{  

		while(!iFile.eof())
		{

			for( int k=0; k < lTotalSegments; k++ ) {

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
						bEOF = true;
					}
				}

				cout << endl;


				/*************************************************/
				// This part of the code looks at the input      //
				// parameters and determines if a packet should  //
				// be simulated lost or damaged. This is also    //
				// known as the GREMLIN function        	 //
				/*************************************************/

				//Send
				bSent = false;
				pPackets[k] = constructPacket(buff, strlen(buff));
			}
		}

		iFile.close();
	}

	cout << "deleting" << endl;

	for(int i=0; i < lTotalSegments; i++) {
		delete pPackets[i];
		delete pTemps[i];
	}

	cout << "made it" << endl;
}