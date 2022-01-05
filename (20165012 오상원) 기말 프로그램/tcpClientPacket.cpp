#include <stdio.h>
#include <WinSock2.h>
#include <ws2tcpip.h>

#include "wtpipv6.h"
#include "wspiapi.h"

#include "Packet.h"


//**********************************************************************
void usage(char *progname)
{
    fprintf(stderr, "usage: %s [-n name] [-p port] \n", progname);
    fprintf(stderr, "   -n name    Host name to resolve, [127.0.0.1] \n");
    fprintf(stderr, "   -p port    Port number to resolve, [8600] \n");
  
    ExitProcess(-1);
}


int resolveAddr(int argc, char **argv, char *serverName, char *serverPort)
{
	int count, rc, i;

	serverName[0]=0;
	serverPort[0]=0;

    for(i=1; i < argc ;i++)
    {
        if ((argv[i][0] != '-') && (argv[i][0] != '/') && (strlen(argv[i]) < 2))
            usage(argv[0]);

        switch (tolower(argv[i][1]))
        {

            case 'n':       // name to resolve
                if (i+1 >= argc)
                    usage(argv[0]);
                strcpy(serverName, argv[++i]);
                break;

            case 'p':       // port/service to resolve
                if (i+1 >= argc)
                    usage(argv[0]);
                strcpy(serverPort, argv[++i]);
                break;


            default:
                usage(argv[0]);
                break;
        }
    }
	if(serverName[0]==0)
		strcpy(serverName, "127.0.0.1");
	if(serverPort[0]==0)
		strcpy(serverPort, "8600");

	printf("** Resolve Address %s:%s \n", serverName, serverPort);


	//******************* remoteHost = gethostbyname(host_name);

	struct hostent *remoteHost;

	remoteHost = gethostbyname(serverName);	// ns.cau.ac.kr
	printf(">> gethostbyname Official name: %s", remoteHost->h_name);

    struct in_addr addr;

	addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];
	printf("-(%s) \n", inet_ntoa(addr));


	if(!isalpha(serverName[0]))
	{
		struct in_addr addr = { 0 };
		addr.s_addr = inet_addr(serverName); 	// 127.0.0.1
		remoteHost = gethostbyaddr((char *) &addr, 4, AF_INET);
		printf(">> gethostbyaddr Official name: %s\n", remoteHost->h_name);
	}

	//*****************************************************************************

    // Resolve the name/address - first assume that the name might be a string
    // literal address
    struct addrinfo  hints, *res=NULL, *ptr;


    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family    = AF_INET;
    hints.ai_socktype  = SOCK_STREAM;
    hints.ai_protocol  = IPPROTO_TCP;

    rc = getaddrinfo( serverName, serverPort, &hints, &res);
    if (rc != 0)
    {
        if (rc == WSAHOST_NOT_FOUND)
        {
            hints.ai_flags = AI_CANONNAME;
			printf("** AI_CANONNAME\n");
            rc = getaddrinfo( serverName, serverPort, &hints, &res );
            if (rc != 0)
            {
                fprintf(stderr, "getaddrinfo failed: %d\n", rc);
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "getaddrinfo failed: %d\n", rc);
            return -1;
        }
    }

    // Count how many addresses were returned
    count = 0;
    ptr   = res;
    while (ptr)
    {
        count++;
        ptr = ptr->ai_next;
    }
    printf("** Hostname '%s' resolved to %d addresses\n", serverName, count);


    //Do a reverse lookup on each of the resolved address
    char numerichost[NI_MAXHOST];
    
	rc = getnameinfo( res->ai_addr, res->ai_addrlen, numerichost, NI_MAXHOST, NULL, 0, NI_NUMERICHOST ); // NI_NAMEREQD, NI_NUMERICHOST
    if (rc != 0)
    {
        fprintf(stderr, "getnameinfo failed: %d\n", rc);
        return -1;
    }
    strcpy( serverName, numerichost);

    printf(">> Numeric address resolved: %s:%s\n", serverName, serverPort);
 
    // Free up the results from getaddrinfo
    freeaddrinfo(res);

	return 1;
}
//************************************************************************

void CommRecv(char *recvData);

DWORD WINAPI NetReceive(LPVOID socketConnect)
{
	char recvBuffer[127];
	int  RecvBytes;

	Packet	recvPacket;
	char	receiveBuffer[PACKETBUFFERSIZE];
	int		receivedPacketSize=0;


	while(1){

		///////////////////////////////////////////////////////
		int bufSize = PACKETBUFFERSIZE - receivedPacketSize;

		if((RecvBytes=recv((SOCKET)socketConnect, &(receiveBuffer[receivedPacketSize]), bufSize, 0))<1){
			::shutdown( (SOCKET) socketConnect, SD_BOTH );
			::closesocket( (SOCKET) socketConnect);
			socketConnect= (LPVOID) INVALID_SOCKET;
			break;
		}
		receivedPacketSize += RecvBytes;

		while( receivedPacketSize > 0 )	// parsing Packet Length
		{
			recvPacket.copyToBuffer( receiveBuffer, receivedPacketSize);
			int packetlength=( int )recvPacket.getPacketSize();

			if( receivedPacketSize >= packetlength)
			{
				//  Parsing, main routine 
				recvPacket.readData(recvBuffer, recvPacket.getDataFieldSize() );
				printf( "(%d Bytes, ID=%d) %s\n", recvPacket.getDataFieldSize(), recvPacket.id(), recvBuffer );
				CommRecv(recvBuffer);

				receivedPacketSize -= packetlength;
				if( receivedPacketSize > 0 )
				{
					::CopyMemory( recvBuffer, ( receiveBuffer + recvPacket.getPacketSize() ), receivedPacketSize );
					::CopyMemory( receiveBuffer, recvBuffer, receivedPacketSize );
				}
			}
			else{// if(recvPacket.id()==0){
				printf( "(%d Bytes, ID=%d) %s\n", recvPacket.getDataFieldSize(), recvPacket.id(), receiveBuffer );
//				receivedPacketSize=0;
				break;
			}
		}

		///////////////////////////////////////////////////////

	}
	
	return NULL;
}

SOCKET socketConnect=INVALID_SOCKET;

void CommInit(int argc, char **argv)
{
	WSADATA wsaData;
	struct sockaddr_in serverAddr;
	HANDLE handleThread;


	::WSAStartup( 0x202, &wsaData );


	socketConnect = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( socketConnect == INVALID_SOCKET )
	{
		printf( "Cannot create socket !!\n");
	}


	//******************************* Address and port resolve
	char serverName[120], serverPort[120];

	if(resolveAddr(argc, argv, serverName, serverPort)<1){
		printf("*** Unable to resolve server name !\n");
	    ExitProcess(-1);
	}

	//  접속할 서버의 정보를 설정한다.
	::memset( &serverAddr, 0, sizeof( serverAddr ) );
	serverAddr.sin_family		= AF_INET;
	serverAddr.sin_addr.s_addr	= ::inet_addr( serverName );
	serverAddr.sin_port			= ::htons( atoi(serverPort) );

	//********************************************************


	if( socketConnect != INVALID_SOCKET){
		if(::connect( socketConnect, ( struct sockaddr* )&serverAddr, sizeof( serverAddr ) ) == SOCKET_ERROR )	{
//			printf( "Cannot connect to server !!\n");
			socketConnect = INVALID_SOCKET;
			ExitProcess(-1);
			}
		else{
			// create thread for receive
			handleThread =CreateThread(NULL, 0, NetReceive, (void *) socketConnect, THREAD_PRIORITY_NORMAL, NULL);
		}
	}

}

void CommSend(char *sending)
{
	char sendData[200];
	Packet sendPacket;
	int sentBytes;

	if(socketConnect == INVALID_SOCKET)
		return;

	sprintf(sendData, "%s", sending);

	sendPacket.clear();
	sendPacket.id(1001);
	sendPacket.writeData( sendData, strlen(sendData)+1 );

	sentBytes = ::send( socketConnect, sendPacket.getPacketBuffer(), sendPacket.getPacketSize(), 0 );

	if(sentBytes<0){
		::shutdown( socketConnect, SD_BOTH );
		::closesocket( socketConnect);
		socketConnect=INVALID_SOCKET;
	}
}

void comHandle(char *recvBuf);

void CommRecv(char *recvData)
{
	comHandle(recvData);
}



