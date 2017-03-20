#include "Rtsp.h"
#include "Rtp.h"
#include "RtspServer.h"

int main(int args,char *argv[])
{
//	printf("InputFileNumber:%d\n",args-1);
//	printf("InputFileName:%s\n",argv[args-1]);
//	Rtsp(argv[args-1]);
    RtspServer server;
    server.startupServer();
    
    getchar();
    
    server.shutdownServer();
	return 0;
}


