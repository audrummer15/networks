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

/* PORTS ASSIGNED TO GROUP
	10034 - 10037 
*/

// GLOBAL DATA
struct sockaddr_in myaddr;      		/* our address */
struct sockaddr_in remaddr;     		/* remote address */
socklen_t addrlen = sizeof(remaddr);        /* length of addresses */
int recvlen;                    		/* # bytes received */
int fd;                         		/* our socket */
unsigned char buf[BUFSIZE];    		/* receive buffer */
int seqnum;
char data[BUFSIZE - 2];

// ACK and NAK constants
const char nak[1] = {0};
const char ack[1] = {1};

char generateChecksum( char* data, int length );
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

    /* now loop, receiving data */
    for (;;) {
			cout<<"\n- Reliable FTP Application -\n";
			cout<<"Waiting on port: "<< PORT << "\n";
            recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

            if (recvlen > 0) {

		//Add data to buffer (minus two byte header)
                for( int x = 2; x < recvlen; x++) {
                	data[x - 2] = buf[x];
					cout << buf[x];
                }
				cout << endl;

		//Make checksum
                if ( generateChecksum(data, BUFSIZE - 2) != buf[1]) {
					cout << "Checksum invalid - NAK\n";
                	sendto(fd, nak, strlen(nak), 0, (struct sockaddr *)&remaddr, addrlen);
                }
                else {
					string command(data);
					if ( command.substr(0,2) == "PUT" ) {
						cout << "Checksum valid - ACK\n";
						seqnum = buf[0];
                		sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&remaddr, addrlen);
						receiveData();
					}
					else {
						cout << "Not a PUT command - NAK\n";
				        		sendto(fd, nak, strlen(nak), 0, (struct sockaddr *)&remaddr, addrlen);
					}
				}
			}

    }
    /* never exits */
}

char generateChecksum( char* data, int length ) {
   char retVal = 0x00;

   for( int i=0; i < length; i++ ) {
       retVal += *(data + i);
   }

   retVal = ~retVal;

   return retVal;
}

void receiveData() {
	ofstream outFile;
	outFile.open("TestFile.txt",fstream::out | fstream::trunc);
	
	recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
	
	while ( recvlen > 1 ) {
		//Make checksum
		if ( generateChecksum(data, BUFSIZE - 2) != buf[1]) {
			cout << "Checksum invalid - NAK - Sequence Num: " << buf[0] << "\n";
			sendto(fd, nak, strlen(nak), 0, (struct sockaddr *)&remaddr, addrlen);
		}
		else {
			cout << "ACK - Seq Num: " << buf[0] << "\n";
			sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&remaddr, addrlen);
			//Add data to buffer (minus two byte header)
			for( int x = 2; x < recvlen; x++) {
				data[x - 2] = buf[x];
				if ( x < 50 ) {
					cout << buf[x];
				}
			}
			cout << endl;
			string buffer(data);
			if ( seqnum != buf[0] ) {
				seqnum = buf[0];
				outFile << buffer;
			}
		}
	}
	outFile.close();
}
