#include "Rtsp.h"
#include "Rtp.h"

//RTSP response
char *RtspResponse = "RTSP/1.0 200 OK\r\n";
char *RtspServer = "Server: VLC\r\n";
char *RtspCachControl = "Cache-Control: no-cache\r\n";
char *RtspSession = "Session: ee62ba70a1ddca;timeout=60\r\n";
char *RtspContentLength = "Content-Length: %d\r\n";
char *RtspCseq = "Cseq: %d\r\n";
char *RtpClientPort;
int RtspCseqNumber = 2;
//RTP Thread use
bool lock;
struct RtpData RtpParameter;
//RTSP receive BUFFER
char recvBuf[BUF_SIZE];
char sendBuf[BUF_SIZE];

//function define
char *getRequestType(char *recvBuffer, char **rtspUrl);
int getCSeqNumber(char *recvBuffer);
char *makeSDP(char *urlBase);


void Rtsp(char *fileName)
{
	//process OPTIONS DESCRIBE SETUP PLAY TEARDOWN
	char *RequestType = NULL;
	char *RtspUrl = NULL;
	int serverFD,clientFD;//Server and Client SocketID
	struct sockaddr_in addrClient;
	socklen_t addrClientLen = sizeof(addrClient);
	int retVal;

    fileName = "/Users/MOMO/ffmpeg/byr.h264";
	printf("fileName=%s\n", fileName);
	//RtspSocket
	createRtspSocket(&serverFD, &clientFD, &addrClient);

	while(1)
	{
		retVal = (int)recv(clientFD, recvBuf, BUF_SIZE, 0);
        printf("%s\n", recvBuf);
		if(retVal == -1){
			printf("Received failed!!\n");
			close(serverFD);//close Socket
			close(clientFD);//close Socket
			exit(0);
		}
		RequestType = getRequestType(recvBuf, &RtspUrl);
        int iCseq = getCSeqNumber(recvBuf);
		printf("RequestType=%s,RtspUrl=%s\n",RequestType,RtspUrl);
		//printf("RecvBuf :\n%s",recvBuf);
		if(!strcmp(RequestType, "OPTIONS"))
			OPTIONS_Reply(clientFD, iCseq);
		else if(!strcmp(RequestType, "DESCRIBE"))
			DESCRIBE_Reply(clientFD, iCseq, RtspUrl);
		else if(!strcmp(RequestType, "SETUP"))
			SETUP_Reply(clientFD, iCseq);
		else if(!strcmp(RequestType, "PLAY"))
			PLAY_Reply(clientFD, iCseq, addrClient, RtspUrl, fileName);
		else if(!strcmp(RequestType, "GET_PARAMETER"))
			GET_PARAMETER_Reply(clientFD, iCseq);
		else if(!strcmp(RequestType, "TEARDOWN"))
			TEARDOWN_Reply(clientFD, iCseq);
		else
		{
			close(clientFD);//close Socket
			printf("Server is listening.....\n");
			clientFD = accept(serverFD,(struct sockaddr*)&addrClient, &addrClientLen);
			if(clientFD == -1){
				perror("Accept failed!!\n");
				close(serverFD);//close Socket
				exit(0);
			}else printf("succeed!!\n");
			RtspCseqNumber = 1;
		}
		RtspCseqNumber++;
		free(RequestType);
        free(RtspUrl);
	}
	free(fileName);
	system("pause");
	exit(1);

}

char *getRequestType(char *recvBuffer, char **rtspUrl)
{
    char temp[BUF_SIZE] = {0};
    memcpy(temp, recvBuf, BUF_SIZE);
    strtok(temp, " ");
    int requestTypeLen = (int)strlen(temp);
    char *requestType = (char*)malloc(requestTypeLen + 1);
    memcpy(requestType, temp, requestTypeLen);
    requestType[requestTypeLen] = '\0';
    char *url = strtok(NULL, " ");
    int urlLen = (int)strlen(url);
    *rtspUrl = (char*)malloc(urlLen + 1);
    memcpy(*rtspUrl, url, urlLen);
    (*rtspUrl)[urlLen] = '\0';
    return requestType;
}

int getCSeqNumber(char *recvBuffer)
{
    char temp[BUF_SIZE] = {0};
    memcpy(temp, recvBuf, BUF_SIZE);
    char *beg = strstr(temp, "CSeq:") + 5;
    char *end = strstr(beg, "\r\n");
    int len = (int)(end - beg);
    char *cseq = (char*)malloc(len + 1);
    memcpy(cseq, beg, len);
    int iCseq = atoi(cseq);
    free(cseq);
    cseq = NULL;
    return iCseq;
}

void createRtspSocket(int *serverFD, int *clientFD, struct sockaddr_in *addrClient)
{
	struct sockaddr_in addrServer;
	socklen_t addrClientLen = sizeof(*addrClient);

	//Create Socket
	*serverFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//Server Socket IP
	addrServer.sin_family = AF_INET;
	addrServer.sin_addr.s_addr = INADDR_ANY;
	addrServer.sin_port = htons(RtspServerPort);

    //reuse
    int option = 1;
    if (setsockopt(*serverFD, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
        perror("reuse failed!\n");
        close(*serverFD);//close Socket
        exit(0);
    }
    
	//Bind Socket
	if(bind(*serverFD, (struct sockaddr*)&addrServer, sizeof(addrServer)) == -1){
		perror("Bind failed!\n");
		close(*serverFD);//close Socket
		exit(0);
	}
	//listen
	if(listen(*serverFD,10) == -1){
		perror("Listen failed!!\n");
		close(*serverFD);//close Socket
		exit(0);
	}
	//wait for connect
	printf("Server is listening.....");
	*clientFD = accept(*serverFD, (struct sockaddr*)addrClient, &addrClientLen);
	if(*clientFD == -1){
		perror("Accept failed!!\n");
		close(*serverFD);//close Socket
		exit(0);
	}
	else printf("succeed!!\n");
}

int str2int(char* temp)
{
    return atoi(temp);
}

char* getRtpClientPort()
{
    printf("\n******getRtpClientPort******\n");
    char *begin = strstr(recvBuf, "client_port=");
    begin = begin + 12;
    
    char *retVal = malloc(11);
    memcpy(retVal, begin, 11);
    printf("retVal=%s\n",retVal);
    
    return retVal;
}

void createRtpThread(char* fileName)
{
	int res;
	pthread_t tid;
	void *thread_result = NULL;

	res = pthread_create(&tid,NULL,Rtp,(void*)fileName);
	if(res!=0){
		perror("Thread creation failed");
		exit(EXIT_FAILURE);
	}
}

void OPTIONS_Reply(int clientFD, int cseq)
{
	char *RtspPublic = "Public: DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE,GET_PARAMETER\r\n\r\n";
    char temp[BUF_SIZE] = {0};

	printf("\n*****OPTIONS*****\n");
    memcpy(temp, RtspResponse, strlen(RtspResponse));
	memcpy(temp + strlen(temp), RtspServer, strlen(RtspServer));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    sprintf(temp, temp, 0);
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    sprintf(temp, temp, cseq);
    memcpy(temp + strlen(temp), RtspPublic, strlen(RtspPublic));

	printf("%s\n",temp);

	strcpy(sendBuf,temp);
	if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
		printf("Sent failed1!!\n");
		close(clientFD);//close Socket
		exit(0);
	}
	bzero(sendBuf,BUF_SIZE);

}

char *makeSDP(char *urlBase)
{
    char temp_a[1024] = {0};
    char *sdp_a = "v=0\r\no=- %ld %ld IN IP4 %s\r\ns=Live stream from mobile\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\n";
    srand((unsigned)time(NULL));
    int verid = rand();
    sprintf(temp_a, sdp_a, verid, verid, urlBase);
    
    char temp_b[1024] = {0};
    char *sdp_b = "m=video 0 RTP/AVP 96\r\nb=TIAS:%d\r\na=maxprate:%d.0000\r\na=control:streamid=1\r\n";
    int max_packet_size = 1200;
    int bitrate = 500000;
    int packets = (bitrate / (max_packet_size * 8)) + 1;
    sprintf(temp_b, sdp_b, bitrate, packets);
    
    char temp_c[1024] = {0};
    char *sdp_c = "a=rtpmap:96 H264/90000\r\na=mimetype:string;\"video/H264\"\r\na=framesize:96 %d-%d\r\na=Width:integer;%d\r\na=Height:integer;%d\r\n";
    int width = 640;
    int height = 480;
    sprintf(temp_c, sdp_c, width, height, width, height);
    
    char temp_d[1024] = {0};
    char *sdp_d = "a=fmtp:96 packetization-mode=1;profile-level-id=%s;sprop-parameter-sets=%s,%s\r\n";
    char *profile = "64001f";
    char *sps = "Z2QAH6zZQFAFuwEQACi7EAmJaAjxgjlg";
    char *pps = "aOvjyyLA";
    sprintf(temp_d, sdp_d, profile, sps, pps);

    char *temp = (char *)malloc(strlen(temp_a) + strlen(temp_b) + strlen(temp_c) + strlen(temp_d) + 1);
    memcpy(temp, temp_a, strlen(temp_a));
    memcpy(temp + strlen(temp), temp_b, strlen(temp_b));
    memcpy(temp + strlen(temp), temp_c, strlen(temp_c));
    memcpy(temp + strlen(temp), temp_d, strlen(temp_d));

    return temp;
}

void DESCRIBE_Reply(int clientFD, int cseq, char *RtspContentBase)
{
	char *RtspContentType = "Content-type: application/sdp\r\n";
    char temp[BUF_SIZE] = {0};
    char *sdp = makeSDP("172.16.69.126:8554");
    
	printf("\n*****DESCRIBE*****\n");
    memcpy(temp, RtspResponse, strlen(RtspResponse));
    memcpy(temp + strlen(temp), RtspServer, strlen(RtspServer));
    memcpy(temp + strlen(temp), RtspContentType, strlen(RtspContentType));
    memcpy(temp + strlen(temp), "Content-Base: ", strlen("Content-Base: "));
    memcpy(temp + strlen(temp), RtspContentBase, strlen(RtspContentBase));
    memcpy(temp + strlen(temp), "\r\n", strlen("\r\n"));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    char temp2[BUF_SIZE] = {0};
    sprintf(temp2, temp, (int)strlen(sdp));
    memcpy(temp, temp2, strlen(temp2));
    memcpy(temp + strlen(temp), RtspCachControl, strlen(RtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    sprintf(temp, temp, cseq);
    memcpy(temp + strlen(temp), "\r\n", strlen("\r\n"));
    memcpy(temp + strlen(temp), sdp, strlen(sdp));

	printf("%s\n",temp);
	strcpy(sendBuf,temp);
	if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
		printf("Sent failed2!!\n");
		close(clientFD);//close Socket
		exit(0);
	}
	bzero(sendBuf,BUF_SIZE);
}
void SETUP_Reply(int clientFD, int cseq)
{
	char *RtspTransport = "Transport: RTP/AVP/UDP;unicast;";
	char *ssrc = ";ssrc=15F6B7CF;";
	char *mode = "mode=play\r\n";
	char *RTPServerPort = ";server_port=50000-50001";
    char temp[BUF_SIZE] = {0};

	RtpClientPort = getRtpClientPort();
	printf("\n*****SETUP*****\n");
    memcpy(temp, RtspResponse, strlen(RtspResponse));
    memcpy(temp + strlen(temp), RtspServer, strlen(RtspServer));
    memcpy(temp + strlen(temp), RtspTransport, strlen(RtspTransport));
    memcpy(temp + strlen(temp), "client_port=", strlen("client_port="));
    memcpy(temp + strlen(temp), RtpClientPort, strlen(RtpClientPort));
    memcpy(temp + strlen(temp), RTPServerPort, strlen(RTPServerPort));
    memcpy(temp + strlen(temp), ssrc, strlen(ssrc));
    memcpy(temp + strlen(temp), mode, strlen(mode));
    memcpy(temp + strlen(temp), RtspSession, strlen(RtspSession));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    sprintf(temp, temp, 0);
    memcpy(temp + strlen(temp), RtspCachControl, strlen(RtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    sprintf(temp, temp, cseq);
    memcpy(temp + strlen(temp), "\r\n", strlen("\r\n"));

	printf("%s\n",temp);

	strcpy(sendBuf,temp);
	if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
		printf("Sent failed3!!\n");
		close(clientFD);//close Socket
		exit(0);
	}
	//ZeroMemory(sendBuf,BUF_SIZE);
	
}
void PLAY_Reply(int clientFD, int cseq, struct sockaddr_in addrClient, char *RtspUrl, char *fileName)
{
	char *RTPInfo = "RTP-Info: url=";
	char *seq = ";seq=5873;";
	char *rtptime = "rtptime=2217555422\r\n";
	char *Range = "Range: npt=10-\r\n";
    char temp[BUF_SIZE] = {0};
	//RTP need params
	//struct RtpData *RtpParameter;
	//RtpParameter = (RtpData*)malloc(sizeof(RtpData));

	printf("\n*****Play*****\n");
    memcpy(temp, RtspResponse, strlen(RtspResponse));
    memcpy(temp + strlen(temp), RtspServer, strlen(RtspServer));
    memcpy(temp + strlen(temp), RTPInfo, strlen(RTPInfo));
    memcpy(temp + strlen(temp), RtspUrl, strlen(RtspUrl));
    memcpy(temp + strlen(temp), seq, strlen(seq));
    memcpy(temp + strlen(temp), rtptime, strlen(rtptime));
    memcpy(temp + strlen(temp), Range, strlen(Range));
    memcpy(temp + strlen(temp), RtspSession, strlen(RtspSession));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    sprintf(temp, temp, 0);
    memcpy(temp + strlen(temp), RtspCachControl, strlen(RtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    sprintf(temp, temp, cseq);
    memcpy(temp + strlen(temp), "\r\n", strlen("\r\n"));
    
	printf("%s\n",temp);
	strcpy(sendBuf,temp);
	if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
		printf("Sent failed4!!\n");
		close(clientFD);//close Socket
		exit(0);
	}
	lock = 1;
	bzero(sendBuf,BUF_SIZE);
	//send rtp
	RtpParameter.addrClient = addrClient;
	RtpParameter.rtpServerPort = RtpServerPort;
	RtpParameter.rtpClientPort = str2int(RtpClientPort);
	createRtpThread(fileName);
}
void GET_PARAMETER_Reply(int clientFD, int cseq)
{
    char temp[BUF_SIZE] = {0};

	printf("\n*****GET_PARAMETER*****\n");
    memcpy(temp, RtspResponse, strlen(RtspResponse));
    memcpy(temp + strlen(temp), RtspServer, strlen(RtspServer));
    memcpy(temp + strlen(temp), RtspSession, strlen(RtspSession));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    sprintf(temp, temp, 0);
    memcpy(temp + strlen(temp), RtspCachControl, strlen(RtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    sprintf(temp, temp, cseq);
    memcpy(temp + strlen(temp), "\r\n", strlen("\r\n"));

	printf("%s\n",temp);
	strcpy(sendBuf,temp);
	if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
		printf("Sent failed0000!!\n");
		close(clientFD);//close Socket
		exit(0);
	}
	bzero(sendBuf,BUF_SIZE);
}
void TEARDOWN_Reply(int clientFD, int cseq)
{
    char temp[BUF_SIZE] = {0};

	printf("\n*****TEARDOWN*****\n");
    memcpy(temp, RtspResponse, strlen(RtspResponse));
    memcpy(temp + strlen(temp), RtspServer, strlen(RtspServer));
    memcpy(temp + strlen(temp), RtspSession, strlen(RtspSession));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    sprintf(temp, temp, 0);
    memcpy(temp + strlen(temp), RtspCachControl, strlen(RtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    sprintf(temp, temp, cseq);
    memcpy(temp + strlen(temp), "\r\n", strlen("\r\n"));

	printf("%s\n",temp);
	strcpy(sendBuf,temp);
	if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
		printf("Sent failed5!!\n");
		close(clientFD);//close Socket
		exit(0);
	}
	lock = 0;
	bzero(sendBuf,BUF_SIZE);
	bzero(recvBuf,BUF_SIZE);
}
