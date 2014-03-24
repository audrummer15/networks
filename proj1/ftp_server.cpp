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

using namespace std;

#define PORT 10035
#define BUFSIZE 128
#define DATASIZE 126
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

Packet* pPacket = new Packet;

unsigned char generateChecksum( Packet*); 
void receiveData();

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
    for (;;) {
        recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

	    memcpy(pPacket, buf, BUFSIZE);

            if (recvlen > 0) {

		//Add data to buffer (minus two byte header)
                for( int x = HEADERSIZE; x < recvlen; x++) {
                	data[x - HEADERSIZE] = buf[x];
					cout << buf[x];
                }
				cout << endl;
	
		//Make checksum
                if ( generateChecksum(pPacket) != buf[1]) {
					cout << "Checksum invalid - NAK\n";
					nak[1] = seqnum;
                	sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
                }
                else {
					string command(data);
					if ( command.substr(0,3) == "PUT" ) {
						cout << "Checksum valid - ACK\n";
						seqnum = pPacket->Sequence;
						ack[1] = seqnum;
                		sendto(fd, ack, 2, 0, (struct sockaddr *)&remaddr, addrlen);
						receiveData();
						cout << "PUT command success." << endl;
					}
					else {
						cout << "Not a PUT command - NAK\n";
						nak[1] = (char)pPacket->Sequence ^ 30;
				        sendto(fd, nak, 2, 0, (struct sockaddr *)&remaddr, addrlen);
					}
				}
			}

    }
    /* never exits */
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
}
