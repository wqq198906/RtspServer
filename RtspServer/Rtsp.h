#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef _RTSP_H_
#define _RTSP_H

#define BUF_SIZE 1024
#define RtspServerPort 8554
#define RtpServerPort 50000

void Rtsp(char *fileName);
void OPTIONS_Reply(int clientFD, int cseq);
void DESCRIBE_Reply(int clientFD, int cseq, char *RtspContentBase);
void SETUP_Reply(int clientFD, int cseq);
void PLAY_Reply(int clientFD, int cseq, struct sockaddr_in addrClient, char *RtspUrl, char *fileName);
void GET_PARAMETER_Reply(int clientFD, int cseq);
void TEARDOWN_Reply(int clientFD, int cseq);
void createRtspSocket(int *serverFD, int *clientFD, struct sockaddr_in *addrClient);

#endif
