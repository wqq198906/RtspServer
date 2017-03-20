//
//  RtspServer.cpp
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/13.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#include "RtspServer.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>

#pragma mark - define

char *rtspResponse = "RTSP/1.0 200 OK\r\n";
char *rtspServer = "Server: MomoAid\r\n";
char *rtspPublic = "Public: DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE,GET_PARAMETER\r\n\r\n";
char *rtspCachControl = "Cache-Control: no-cache\r\n";
char *rtspSession = "Session: %s;timeout=60\r\n";
char *RtspContentLength = "Content-Length: %d\r\n";
char *RtspCseq = "Cseq: %d\r\n";
char *RtspContentType = "Content-type: application/sdp\r\n";
char *RtspTransport = "Transport: RTP/AVP/UDP;unicast;";
char *RTPServerPort = ";server_port=50000-50001";
char *RTSPSegmentation = "\r\n";

char recvBuf[BufferSize];
char sendBuf[BufferSize];

#pragma mark - function declare

void getLocalAddress(char **addr);
void setFormatLength(char **buffer, int length);
char *makeSDP();
char *getRtpClientPort();

void optionsReply(int clientFD, int iCseq);
void desribeReply(int clientFD, int iCseq, char *RtspUrl);
void setupReply(int clientFD, int iCseq);
void playReply(int clientFD, int iCseq, struct sockaddr_in addrClient, char *RtspUrl);
void getParameterReply(int clientFD, int iCseq);
void teardownReply(int clientFD, int iCseq);


#pragma mark - private utils
void getLocalAddress(char **addr)
{
    char hostname[BufferSize] = {0};
    gethostname(hostname, BufferSize);
    struct hostent *phost = gethostbyname(hostname);
    if (phost) {
        char *ip = inet_ntoa(*((struct in_addr *)phost->h_addr_list[0]));
        strncpy(*addr, ip, strlen(ip));
    }
}

void setFormatLength(char **buffer, int length)
{
    char temp[BufferSize] = {0};
    sprintf(temp, *buffer, 0);
    strncpy(*buffer, temp, strlen(temp));
}

char *makeSDP()
{
    char temp_a[1024] = {0};
    char *sdp_a = "v=0\r\no=- %ld %ld IN IP4 %s\r\ns=Live stream from mobile\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\n";
    long verid = random();
    char addr[128] = {0};
    getLocalAddress((char **)&addr);
    char urlBase[128] = {0};
    sprintf(urlBase, "%s:%d", addr, RtspServerPort);
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
    strncpy(temp, temp_a, strlen(temp_a));
    strncpy(temp + strlen(temp), temp_b, strlen(temp_b));
    strncpy(temp + strlen(temp), temp_c, strlen(temp_c));
    strncpy(temp + strlen(temp), temp_d, strlen(temp_d));
    
    return temp;
}

char *getRtpClientPort()
{
    printf("\n******getRtpClientPort******\n");
    char *begin = strstr(recvBuf, "client_port=");
    begin = begin + 12;
    
    char *retVal = malloc(11);
    strncpy(retVal, begin, 11);
    printf("retVal=%s\n",retVal);
    
    return retVal;
}

#pragma mark - private command
/**
 options Reply
 */
void optionsReply(int clientFD, int iCseq)
{
    char temp[BufferSize] = {0};
    
    printf("\n*****OPTIONS*****\n");
    strncpy(temp, rtspResponse, strlen(rtspResponse));
    strncpy(temp + strlen(temp), rtspServer, strlen(rtspServer));
    strncpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    setFormatLength((char **)&temp, 0);
    strncpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    setFormatLength((char **)&temp, iCseq);
    strncpy(temp + strlen(temp), rtspPublic, strlen(rtspPublic));
    
    printf("%s\n",temp);
    
    strncpy(sendBuf, temp, strlen(temp));
    if(send(clientFD, sendBuf, strlen(sendBuf),0) == -1){
        printf("Sent options reply failed!!\n");
        close(clientFD);//close Socket
        exit(0);
    }
    memset(sendBuf, 0, BufferSize);
}

/**
 desribe Reply
 */
void desribeReply(int clientFD, int iCseq, char *RtspUrl)
{
    char temp[BufferSize] = {0};
    char *sdp = makeSDP();
    
    printf("\n*****DESCRIBE*****\n");
    strncpy(temp, rtspResponse, strlen(rtspResponse));
    strncpy(temp + strlen(temp), rtspServer, strlen(rtspServer));
    strncpy(temp + strlen(temp), RtspContentType, strlen(RtspContentType));
    strncpy(temp + strlen(temp), "Content-Base: ", strlen("Content-Base: "));
    strncpy(temp + strlen(temp), RtspUrl, strlen(RtspUrl));
    strncpy(temp + strlen(temp), RTSPSegmentation, strlen(RTSPSegmentation));
    strncpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    setFormatLength((char **)&temp, (int)strlen(sdp));
    strncpy(temp + strlen(temp), rtspCachControl, strlen(rtspCachControl));
    strncpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    setFormatLength((char **)&temp, iCseq);
    strncpy(temp + strlen(temp), RTSPSegmentation, strlen(RTSPSegmentation));
    strncpy(temp + strlen(temp), sdp, strlen(sdp));
    
    free(sdp);
    sdp = NULL;
    
    printf("%s\n",temp);
    strcpy(sendBuf,temp);
    if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
        printf("Sent desribe reply failed!!\n");
        close(clientFD);//close Socket
        exit(0);
    }
    memset(sendBuf, 0, BufferSize);
}

/**
 setup Reply
 */
void setupReply(int clientFD, int iCseq)
{
    char ssrc[128] = {0};
    sprintf(ssrc, ";ssrc=%lx;", random());
    char *mode = "mode=play\r\n";
    char temp[BufferSize] = {0};
    
    char *rtpClientPort = getRtpClientPort();
    printf("\n*****SETUP*****\n");
    strncpy(temp, rtspResponse, strlen(rtspResponse));
    strncpy(temp + strlen(temp), rtspServer, strlen(rtspServer));
    strncpy(temp + strlen(temp), RtspTransport, strlen(RtspTransport));
    strncpy(temp + strlen(temp), "client_port=", strlen("client_port="));
    strncpy(temp + strlen(temp), rtpClientPort, strlen(rtpClientPort));
    strncpy(temp + strlen(temp), RTPServerPort, strlen(RTPServerPort));
    strncpy(temp + strlen(temp), ssrc, strlen(ssrc));
    strncpy(temp + strlen(temp), mode, strlen(mode));
    strncpy(temp + strlen(temp), rtspSession, strlen(rtspSession));
    strncpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    setFormatLength((char **)&temp, 0);
    strncpy(temp + strlen(temp), rtspCachControl, strlen(rtspCachControl));
    strncpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    setFormatLength((char **)&temp, iCseq);
    strncpy(temp + strlen(temp), RTSPSegmentation, strlen(RTSPSegmentation));
    
    printf("%s\n",temp);
    
    free(rtpClientPort);
    rtpClientPort = NULL;
    
    strcpy(sendBuf,temp);
    if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
        printf("Sent setup reply failed!!\n");
        close(clientFD);//close Socket
        exit(0);
    }
    memset(sendBuf, 0, BufferSize);
}

/**
 play Reply
 */
void playReply(int clientFD, int iCseq, struct sockaddr_in addrClient, char *RtspUrl)
{
    char *RTPInfo = "RTP-Info: url=";
    char *seq = ";seq=5873;";
    char *rtptime = "rtptime=2217555422\r\n";
    char *Range = "Range: npt=10-\r\n";
    char temp[BufferSize] = {0};
    //RTP need params
    //struct RtpData *RtpParameter;
    //RtpParameter = (RtpData*)malloc(sizeof(RtpData));
    
    printf("\n*****Play*****\n");
    memcpy(temp, rtspResponse, strlen(rtspResponse));
    memcpy(temp + strlen(temp), rtspServer, strlen(rtspServer));
    memcpy(temp + strlen(temp), RTPInfo, strlen(RTPInfo));
    memcpy(temp + strlen(temp), RtspUrl, strlen(RtspUrl));
    memcpy(temp + strlen(temp), seq, strlen(seq));
    memcpy(temp + strlen(temp), rtptime, strlen(rtptime));
    memcpy(temp + strlen(temp), Range, strlen(Range));
    memcpy(temp + strlen(temp), rtspSession, strlen(rtspSession));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    setFormatLength((char **)&temp, 0);
    memcpy(temp + strlen(temp), rtspCachControl, strlen(rtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    setFormatLength((char **)&temp, iCseq);
    memcpy(temp + strlen(temp), "\r\n", strlen("\r\n"));
    
    printf("%s\n",temp);
    strcpy(sendBuf,temp);
    if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
        printf("Sent play reply failed!!\n");
        close(clientFD);//close Socket
        exit(0);
    }
    memset(sendBuf, 0, BufferSize);
    //send rtp
//    lock = 1;
//    RtpParameter.addrClient = addrClient;
//    RtpParameter.rtpServerPort = RtpServerPort;
//    RtpParameter.rtpClientPort = str2int(RtpClientPort);
//    createRtpThread(fileName);
}

/**
 getParameter Reply
 */
void getParameterReply(int clientFD, int iCseq)
{
    char temp[BufferSize] = {0};
    
    printf("\n*****GET_PARAMETER*****\n");
    memcpy(temp, rtspResponse, strlen(rtspResponse));
    memcpy(temp + strlen(temp), rtspServer, strlen(rtspServer));
    memcpy(temp + strlen(temp), rtspSession, strlen(rtspSession));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    setFormatLength((char **)&temp, 0);
    memcpy(temp + strlen(temp), rtspCachControl, strlen(rtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    setFormatLength((char **)&temp, iCseq);
    memcpy(temp + strlen(temp), RTSPSegmentation, strlen(RTSPSegmentation));
    
    printf("%s\n",temp);
    strcpy(sendBuf,temp);
    if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
        printf("Sent getParameter reply failed!!\n");
        close(clientFD);//close Socket
        exit(0);
    }
    memset(sendBuf, 0, BufferSize);
}

/**
 teardown Reply
 */
void teardownReply(int clientFD, int iCseq)
{
    char temp[BufferSize] = {0};
    
    printf("\n*****TEARDOWN*****\n");
    memcpy(temp, rtspResponse, strlen(rtspResponse));
    memcpy(temp + strlen(temp), rtspServer, strlen(rtspServer));
    memcpy(temp + strlen(temp), rtspSession, strlen(rtspSession));
    memcpy(temp + strlen(temp), RtspContentLength, strlen(RtspContentLength));
    setFormatLength((char **)&temp, 0);
    memcpy(temp + strlen(temp), rtspCachControl, strlen(rtspCachControl));
    memcpy(temp + strlen(temp), RtspCseq, strlen(RtspCseq));
    setFormatLength((char **)&temp, iCseq);
    memcpy(temp + strlen(temp), RTSPSegmentation, strlen(RTSPSegmentation));
    
    printf("%s\n",temp);
    strcpy(sendBuf,temp);
    if(send(clientFD,sendBuf,strlen(sendBuf),0) == -1){
        printf("Sent teardown reply failed!!\n");
        close(clientFD);//close Socket
        exit(0);
    }
    memset(sendBuf, 0, BufferSize);
    memset(recvBuf, 0, BufferSize);
//    lock = 0;
}

#pragma mark - private functions

/**
 */
void createRtspSocket(int *serverFD, int *clientFD, struct sockaddr_in *addrClient)
{
    
}

#pragma mark - public

void initRtspServer()
{
    
}


void startRtspServer()
{
    
}

void stopRtspServer()
{
    
}
