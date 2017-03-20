#include "Rtp.h"

char *FileTemp;
char *RtpHeader;
char *FUIndicator;
char *FUHeader;

void *Rtp(void *fileName)
{
	int sockFD;
	struct sockaddr_in addrClient;
	//RtpHeader params
	int SequenceNumber = 26074;
	unsigned int timestamp = 2785272748;
	unsigned int ssrc = 0xc630ebd7;
	//FileTemp params
	char *FrameStartIndex = NULL;
	int FrameLength = 0;
	int FileSize;

	printf("I'm at Rtp()!\n");
	
	//createRtpSocket
	createRtpSocket(&sockFD, &addrClient);
	FileSize = OpenVideoFile((char*)fileName);
	createRtpHeader();
	setRtpVersion(); 
	setRtpPayloadType();
	setSequenceNumber(SequenceNumber);
	setTimestamp(timestamp);
	setSSRC(ssrc);

	printf("Rtplock=%d\n",lock);
	for(int i=0;i<FileSize && lock;i++){
		//H.264 StartCode 00 00 00 01 or 00 00 01
		if(*((int*)(FileTemp+i))==0x01000000){
			//if((*((int*)(FileTemp+i))&0x00FFFFFF)==0x00010000) continue;
			if(FrameLength>0)
				RtpEncoder(sockFD, addrClient, FrameStartIndex, FrameLength, &SequenceNumber, &timestamp);
			FrameStartIndex = FileTemp + i;
			FrameLength = 0;
			i++;//StartCode(0x00010000)
			FrameLength++;
			//printf("FrameStartIndex=%X\n",*((int*)FrameStartIndex));
		}else if((*((int*)(FileTemp+i))&0x00FFFFFF)==0x00010000){
			if(FrameLength>0)
				RtpEncoder(sockFD,addrClient,FrameStartIndex,FrameLength,&SequenceNumber,&timestamp);
			FrameStartIndex = FileTemp + i;
			FrameLength = 0;
			//printf("FrameStartIndex!!!=%X\n",*((int*)FrameStartIndex));
		}
		FrameLength++;
	}
	printf("Rtplock=%d\n",lock);
	printf("End\n");
	close(sockFD);//close Socket
	free(FileTemp);
	free(RtpHeader);
	free(FUIndicator);
	free(FUHeader);
    
    return NULL;
}

void createRtpSocket(int *sockFD, struct sockaddr_in *addrClient)
{
	struct sockaddr_in addrServer;
	int addrClientLen = sizeof(addrClient);

	//create Socket
	*sockFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(*sockFD == -1){
		perror("Socket failed!!\n");
		exit(0);
	} 
	//Server Socket addr
	addrServer.sin_family = AF_INET;
	addrServer.sin_addr.s_addr = INADDR_ANY;
	addrServer.sin_port = htons(RtpParameter.rtpServerPort);
	//Client Socket addr
	addrClient->sin_family = AF_INET;
	addrClient->sin_addr.s_addr = RtpParameter.addrClient.sin_addr.s_addr;
	addrClient->sin_port = htons(RtpParameter.rtpClientPort);

	//printf("IP=%s\n",inet_ntoa(addrClient->sin_addr));
	//printf("Port=%d\n",ntohs(addrClient->sin_port));
	if(bind(*sockFD, (struct sockaddr*)&addrServer,sizeof(addrServer)) == -1){
		perror("Bind failed!\n");
		close(*sockFD);//close Socket
		exit(0);
	}
}
int OpenVideoFile(char *fileName)
{
	FILE *fPtr;
	int FileSize;

	fPtr = fopen(fileName,"rb");
	if(!fPtr){
		perror("Open failed!");
		exit(0);
	}
	//find the fileSize
	fseek(fPtr,0,SEEK_END);
	FileSize = ftell(fPtr);
	fseek(fPtr,0,SEEK_SET);
	//printf("fileSize=%d bytes\n",FileSize);
	
	FileTemp = (char*)malloc(FileSize*sizeof(char));
	
	if(fPtr)
		fread(FileTemp,1,FileSize,fPtr);
	else 
		perror("File Open Error!"),exit(0);

	fclose(fPtr);
	return FileSize;
}

void createRtpHeader()
{
	int RtpHeaderSize = 12;
	RtpHeader = (char*)malloc(RtpHeaderSize*sizeof(char));
	bzero(RtpHeader,RtpHeaderSize);
}
void setRtpVersion()
{
	RtpHeader[0] |= 0x80;
}
void setRtpPayloadType()
{
	RtpHeader[1] |= PayloadType;
}
void setSequenceNumber(int SequenceNumber)
{
	RtpHeader[2] &=0;
	RtpHeader[3] &=0;
	RtpHeader[2] |= (char)((SequenceNumber & 0x0000FF00)>>8);
	RtpHeader[3] |= (char)(SequenceNumber & 0x000000FF);
	//printf("%X\n%X\n", RtpHeader[2],RtpHeader[3]);
}
void setTimestamp(unsigned int timestamp)
{
	RtpHeader[4] &= 0;
	RtpHeader[5] &= 0;
	RtpHeader[6] &= 0;
	RtpHeader[7] &= 0;
	RtpHeader[4] |= (char)((timestamp & 0xff000000) >> 24);
	RtpHeader[5] |= (char)((timestamp & 0x00ff0000) >> 16);
	RtpHeader[6] |= (char)((timestamp & 0x0000ff00) >> 8);
	RtpHeader[7] |= (char)(timestamp & 0x000000ff);
	/*printf("setTimestamp=%X\n", RtpHeader[4] );
	printf("%X\n", RtpHeader[5] );
	printf("%X\n", RtpHeader[6] );
	printf("%X\n", RtpHeader[7] );*/
}
void setSSRC(unsigned int ssrc)
{
	RtpHeader[8]  &= 0;
	RtpHeader[9]  &= 0;
	RtpHeader[10] &= 0;
	RtpHeader[11] &= 0;
	RtpHeader[8]  |= (char) ((ssrc & 0xff000000) >> 24);
	RtpHeader[9]  |= (char) ((ssrc & 0x00ff0000) >> 16);
	RtpHeader[10] |= (char) ((ssrc & 0x0000ff00) >> 8);
	RtpHeader[11] |= (char) (ssrc & 0x000000ff);
	/*printf("%X\n", RtpHeader[8] );
	printf("%X\n", RtpHeader[9] );
	printf("%X\n", RtpHeader[10] );
	printf("%X\n", RtpHeader[11] );*/
}
void setMarker(int marker)
{
	if(marker == 0){
		RtpHeader[1] &= 0x7f;
	}else{
		RtpHeader[1] |= 0x80;
	}
}

//create rtp
void RtpEncoder(int sockFD, struct sockaddr_in addrClient, char *FrameStartIndex, int FrameLength, int *SequenceNumber, unsigned int *timestamp)
{
	if(*((int*)FrameStartIndex) == 0x01000000) FrameLength -= 4;
	else  FrameLength -= 3;

	if(FrameLength < 1400){
		char *sendBuf = (char*)malloc((FrameLength+12)*sizeof(char));
		
		//[RTP Header][NALU][Raw Date]
		if(*((int*)FrameStartIndex) == 0x01000000) 
			memcpy(sendBuf+12,FrameStartIndex+4,FrameLength);
		else
			memcpy(sendBuf+12,FrameStartIndex+3,FrameLength);

		if(sendBuf[12] == 0x67 || sendBuf[12] == 0x68){
			setMarker(0);
			memcpy(sendBuf,RtpHeader,12);
		}else{
			setMarker(1);
			memcpy(sendBuf,RtpHeader,12);
			(*timestamp) += (90000/fps);
			setTimestamp(*timestamp);
		}
		
		if(sendto(sockFD,sendBuf,FrameLength+12,0,(struct sockaddr *)&addrClient,sizeof(addrClient)) == -1){
			printf("Sent failed!!\n");
			close(sockFD);//close Socket
			free(sendBuf);
		}
		usleep(sleepTime);
		(*SequenceNumber)++;
		setSequenceNumber(*SequenceNumber);
		free(sendBuf);
		//printf("FrameLength=%d\n",FrameLength);
		//printf("FrameStartIndex1=%X\n",*((int*)FrameStartIndex));
	}
    else {
		//[RTP Header][FU indicator][FU header][Raw Data]
		FUIndicator = (char*)malloc(sizeof(char));
		FUHeader = (char*)malloc(sizeof(char));
		if(FrameLength >= 1400){
			setMarker(0);
			setFUIndicator(FrameStartIndex);
			setFUHeader(FrameStartIndex,1,0);
			char *sendBuf = (char *)malloc(1500*sizeof(char));
			memcpy(sendBuf,RtpHeader,12);//[RTP Header]
			memcpy(sendBuf+12,FUIndicator,1);//[FU indicator]
			memcpy(sendBuf+13,FUHeader,1);//[FU header]

			if(FrameStartIndex[3] == 0x01){//00 00 00 01
				memcpy(sendBuf+14,FrameStartIndex+5,1386);//[raw data]
				FrameLength -= 1387;
				FrameStartIndex += 1391;
				//printf("FrameStartIndex2=%X\n",*((int*)FrameStartIndex));
			}else if(FrameStartIndex[2] == 0x01){//00 00 01
				memcpy(sendBuf+14,FrameStartIndex+4,1386);//[raw data]
				FrameLength -= 1387;
				FrameStartIndex += 1390;
				//printf("FrameStartIndex3=%X\n",*((int*)FrameStartIndex));
			}
			if(sendto(sockFD,sendBuf,1400,0,(struct sockaddr *)&addrClient,sizeof(addrClient)) == -1){
				printf("Sent failed!!\n");
				close(sockFD);//close Socket
				free(sendBuf);
			}
			usleep(sleepTime);
			(*SequenceNumber)++;
			setSequenceNumber(*SequenceNumber);
			
			while(FrameLength >= 1400){
				setFUIndicator(FrameStartIndex);
				setFUHeader(FrameStartIndex,0,0);
				memcpy(sendBuf,RtpHeader,12);//[RTP Header]
				memcpy(sendBuf+12,FUIndicator,1);//[FU indicator]
				memcpy(sendBuf+13,FUHeader,1);//[FU header]
				memcpy(sendBuf+14,FrameStartIndex,1386);
				FrameLength -= 1386;
				FrameStartIndex +=1386;
				//printf("FrameStartIndex4=%X\n",*((int*)FrameStartIndex));

				if(sendto(sockFD,sendBuf,1400,0,(struct sockaddr *)&addrClient,sizeof(addrClient)) == -1){
					printf("Sent failed!!\n");
					close(sockFD);//Ãö³¬Socket
					free(sendBuf);
				}
				usleep(sleepTime);
				(*SequenceNumber)++;
				setSequenceNumber(*SequenceNumber);
			}
			if(FrameLength > 0){
				setFUIndicator(FrameStartIndex);
				setFUHeader(FrameStartIndex,0,1);
				setMarker(1);
				memcpy(sendBuf,RtpHeader,12);//[RTP Header]
				memcpy(sendBuf+12,FUIndicator,1);//[FU indicator]
				memcpy(sendBuf+13,FUHeader,1);//[FU header]
				memcpy(sendBuf+14,FrameStartIndex,FrameLength);
				if(sendto(sockFD,sendBuf,FrameLength+14,0,(struct sockaddr *)&addrClient,sizeof(addrClient)) == -1){
					printf("Sent failed!!\n");
					close(sockFD);//Ãö³¬Socket
					free(sendBuf);
				}
				usleep(sleepTime);
				//printf("FrameStartIndex5=%X\n",*((int*)FrameStartIndex));
				(*timestamp) += (90000/fps);
				setTimestamp(*timestamp);
				(*SequenceNumber)++;
				setSequenceNumber(*SequenceNumber);
			}
			free(sendBuf);
		}
	}	
}
void setFUIndicator(char *FrameStartIndex)
{
	int FUIndicatorType = 28;
	
	if(FrameStartIndex[2] == 0x01)
		FUIndicator[0] = (FrameStartIndex[3] & 0x60) | FUIndicatorType;
	else if(FrameStartIndex[3] == 0x01)
		FUIndicator[0] = (FrameStartIndex[4] & 0x60) | FUIndicatorType;
}
void setFUHeader(char *FrameStartIndex,bool start,bool end)
{
	if(FrameStartIndex[3] == 0x01)
		FUHeader[0] = FrameStartIndex[4] & 0x1f;
	else if(FrameStartIndex[2] == 0x01)
		FUHeader[0] = FrameStartIndex[3] & 0x1f;
	/*
	   |0|1|2|3 4 5 6 7|
	   |S|E|R|  TYPE   | 
	*/
	if(start) FUHeader[0] |= 0x80;
	else FUHeader[0] &= 0x7f;
	if(end) FUHeader[0] |= 0x40;
	else FUHeader[0] &= 0xBF;
}
