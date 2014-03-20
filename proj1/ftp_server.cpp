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

char generateChecksum( char* data, int length );

int main()
{
    struct sockaddr_in myaddr;      				/* our address */
    struct sockaddr_in remaddr;     				/* remote address */
    socklen_t addrlen = sizeof(remaddr);            /* length of addresses */
    int recvlen;                    				/* # bytes received */
    int fd;                         				/* our socket */
    unsigned char buf[BUFSIZE];    					/* receive buffer */

    // ACK and NAK constants
    const char nak[1] = {0};
    const char ack[1] = {1};
    

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
            recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

            if (recvlen > 0) {
                char data[BUFSIZE - 2];
                for( int x = 2; x < BUFSIZE; x++) {
                	data[x - 2] = buf[x];
                }

                if ( generateChecksum(data, BUFSIZE - 2) != buf[1]) {
                	sendto(fd, nak, strlen(nak), 0, (struct sockaddr *)&remaddr, addrlen);
                }
                else {
                    sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&remaddr, addrlen);
                }
            }

            // EXAMPLE CODE  
            // printf("received %d bytes\n", recvlen);
            // if (recvlen > 0) {
            //         buf[recvlen] = 0;
            //         printf("received message: \"%s\"\n", buf);
            // }
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
