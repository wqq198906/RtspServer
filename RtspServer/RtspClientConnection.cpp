//
//  RtspClientConnection.cpp
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/14.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#include "RtspClientConnection.h"
#include "RtspMessage.h"
#include <string>
#include <algorithm>
#include <cassert>
#include <sstream>
#include "RtspUtils.h"

RtspClientConnection::RtspClientConnection(int connectFd, sockaddr_in connectAddr, RtspServer *server)
{
    connectFd_ = connectFd;
    connectAddr_ = connectAddr;
    server_ = server;
    isRunning_ = true;
    isSending_ = false;
    
    int res = pthread_create(&connectTid_, nullptr, connectLoop, this);
    if (res != 0) {
        isRunning_ = false;
    }
}

void RtspClientConnection::createAndSendRtpPacket(char *data, int length)
{
    printf("createAndSendRtpPacket length %d\n", length);

    if(*((int*)data) == 0x01000000)
        length -= 4;
    else
        length -= 3;
    
    setRtpPacketTimestamp((unsigned int)timestamp_);
    if (length < 1400) {
        char *sendBuf = (char *)malloc((length + 12) * sizeof(char));
        
        setRtpPacketSequenceNumber(sequenceNumber_);
        //[RTP Header][NALU][Raw Data]
        if(*((int*)data) == 0x01000000)
            memcpy(sendBuf + 12, data + 4, length);
        else //0x010000
            memcpy(sendBuf + 12, data + 3, length);
        
        if(sendBuf[12] == 0x67 || sendBuf[12] == 0x68) {
            setRtpPacketMarker(0);
            memcpy(sendBuf, rtpHeader_, 12);
        }
        else {
            setRtpPacketMarker(1);
            memcpy(sendBuf, rtpHeader_, 12);
        }
        
        ssize_t sendSize = 0;
        if((sendSize = sendto(serverRtpFd_, sendBuf, length + 12, 0, (struct sockaddr *)&connectAddr_, sizeof(connectAddr_))) == -1) {
            printf("sendto failed\n");
            close(serverRtpFd_);//close Socket
            free(sendBuf);
        }
        else {
            printf("sendto1 length %d:%zd\n", length + 12, sendSize);
        }
        sequenceNumber_++;
        free(sendBuf);
        sendBuf = NULL;
    }
    else {
        //[RTP Header][FU indicator][FU header][Raw Data]
        setRtpPacketMarker(0);
        setFUIndicator(data);
        setFUHeader(data, 1, 0);
        char *sendBuf = (char *)malloc(1500 * sizeof(char));
        
        setRtpPacketSequenceNumber(sequenceNumber_);
        memcpy(sendBuf, rtpHeader_, 12);//[RTP Header]
        memcpy(sendBuf + 12, &fuIndicator_, 1);//[FU indicator]
        memcpy(sendBuf + 13, &fuHeader_, 1);//[FU header]
        
        if(data[3] == 0x01) {//00 00 00 01
            memcpy(sendBuf + 14, data + 5, 1386);//[raw data]
            length -= 1387;
            data += 1391;
        }
        else if(data[2] == 0x01) {//00 00 01
            memcpy(sendBuf + 14, data + 4, 1386);//[raw data]
            length -= 1387;
            data += 1390;
        }
        
        ssize_t sendSize = 0;
        if((sendSize = sendto(serverRtpFd_, sendBuf, 1400, 0, (struct sockaddr *)&connectAddr_, sizeof(connectAddr_))) == -1) {
            printf("sendto failed!!\n");
            close(serverRtpFd_);//close Socket
            free(sendBuf);
        }
        else {
            printf("sendto2 length %d:%zd\n", 1400, sendSize);
        }
        
        sequenceNumber_++;
        
        //if left length still large 1400 send it
        while(length >= 1400) {
            setFUIndicator(data);
            setFUHeader(data, 0, 0);
            setRtpPacketSequenceNumber(sequenceNumber_);
            memcpy(sendBuf, rtpHeader_, 12);//[RTP Header]
            memcpy(sendBuf + 12, &fuIndicator_, 1);//[FU indicator]
            memcpy(sendBuf + 13, &fuHeader_, 1);//[FU header]
            memcpy(sendBuf + 14, data, 1386);
            length -= 1386;
            data += 1386;
            
            
            if((sendSize = sendto(serverRtpFd_, sendBuf, 1400, 0, (struct sockaddr *)&connectAddr_, sizeof(connectAddr_))) == -1){
                printf("Sent failed!!\n");
                close(serverRtpFd_);
                free(sendBuf);
            }
            else {
                printf("sendto3 length %d:%zd\n", 1400, sendSize);
            }
            sequenceNumber_++;
        }
        
        //if left data still large 0 send it
        if(length > 0) {
            setFUIndicator(data);
            setFUHeader(data, 0, 1);
            setRtpPacketMarker(1);
            setRtpPacketSequenceNumber(sequenceNumber_);
            memcpy(sendBuf, rtpHeader_, 12);//[RTP Header]
            memcpy(sendBuf + 12, &fuIndicator_, 1);//[FU indicator]
            memcpy(sendBuf + 13 ,&fuHeader_, 1);//[FU header]
            memcpy(sendBuf + 14, data, length);
            if((sendSize = sendto(serverRtpFd_, sendBuf, length + 14, 0, (struct sockaddr *)&connectAddr_, sizeof(connectAddr_))) == -1){
                printf("Sent failed!!\n");
                close(serverRtpFd_);
                free(sendBuf);
            }
            else {
                printf("sendto4 length %d:%zd\n", length + 14, sendSize);
            }
            sequenceNumber_++;
        }
        free(sendBuf);
        sendBuf = NULL;
    }
}

void RtspClientConnection::setRtpPacketSequenceNumber(int number)
{
    rtpHeader_[2] &=0;
    rtpHeader_[3] &=0;
    rtpHeader_[2] |= (char)((number & 0x0000FF00)>>8);
    rtpHeader_[3] |= (char)( number & 0x000000FF);
}

void RtspClientConnection::setRtpPacketTimestamp(unsigned int timestamp)
{
    rtpHeader_[4] &= 0;
    rtpHeader_[5] &= 0;
    rtpHeader_[6] &= 0;
    rtpHeader_[7] &= 0;
    rtpHeader_[4] |= (char)((timestamp & 0xff000000) >> 24);
    rtpHeader_[5] |= (char)((timestamp & 0x00ff0000) >> 16);
    rtpHeader_[6] |= (char)((timestamp & 0x0000ff00) >> 8);
    rtpHeader_[7] |= (char)( timestamp & 0x000000ff);
}

void RtspClientConnection::setRtpPacketMarker(int marker)
{
    if(marker == 0) {
        rtpHeader_[1] &= 0x7f;
    }
    else{
        rtpHeader_[1] |= 0x80;
    }
}

void RtspClientConnection::setFUIndicator(char *data)
{
    int FUIndicatorType = 28;
    
    if(data[2] == 0x01)
        fuIndicator_ = (data[3] & 0x60) | FUIndicatorType;
    else if(data[3] == 0x01)
        fuIndicator_ = (data[4] & 0x60) | FUIndicatorType;
}

void RtspClientConnection::setFUHeader(char *data, bool start, bool end)
{
    if(data[2] == 0x01)
        fuHeader_ = data[3] & 0x1f;
    else if(data[3] == 0x01)
        fuHeader_ = data[4] & 0x1f;
    
    /*
     |0|1|2|3 4 5 6 7|
     |S|E|R|  TYPE   |
     */
    if(start)
        fuHeader_ |= 0x80;
    else
        fuHeader_ &= 0x7f;
    if(end)
        fuHeader_ |= 0x40;
    else
        fuHeader_ &= 0xBF;
}

/**
 data with start code
 */
void RtspClientConnection::onVideoData(char *data, int length, double pts, bool isKey)
{
    if (!isSending_)
        return;
    
    //first packet need I frame
    if (rtpBase_ == 0 && !isKey)
        return;
    
    if (rtpBase_ == 0) {
        rtpBase_ = random() + 10000;
        ptsBase_ = pts;
    }
    
    pts -= ptsBase_;
    uint64_t rtpPts = (uint64_t)(pts * 90000);
    rtpPts += rtpBase_;
    timestamp_ = rtpPts;
    
    printf("onVideoData length %d\n", length);
    int frameLength = 0;
    char *frameStartPtr = NULL;
    char *pdata = data;
    for(int i = 0; i < length; i++) {
        //H.264 StartCode 00 00 00 01 or 00 00 01
        if(*((int*)(pdata + i)) == 0x01000000) {
            if(frameLength > 0)
                createAndSendRtpPacket(frameStartPtr, frameLength);
            frameStartPtr = pdata + i;
            frameLength = 0;
            i++;//StartCode(0x00010000)
            frameLength++;
        }
        frameLength++;
    }
    //if only one startcode
    if(frameLength > 0)
        createAndSendRtpPacket(frameStartPtr, frameLength);
    
}

void RtspClientConnection::shutdown()
{
    if (isRunning_) {
        isRunning_ = false;
        isSending_ = false;
        close(connectFd_);
        pthread_join(connectTid_, nullptr);
        
        close(serverRtpFd_);
        close(serverRtcpFd_);
    }
}

std::string RtspClientConnection::makeSDP()
{
    std::string sdp;
    std::stringstream ss;
    long verid = random();
    
    ss << "v=0\r\no=- "<< verid << " " << verid << " IN IP4 " << urlBase_ << "\r\ns=Live stream from mobile\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\n";
    sdp.append(ss.str());
    ss.str("");
    
    
    int max_packet_size = 1200;
    int bitrate = 500000;
    int packets = (bitrate / (max_packet_size * 8)) + 1;
    ss << "m=video 0 RTP/AVP 96\r\nb=TIAS:" << bitrate << "\r\na=maxprate:" << packets << ".0000\r\na=control:streamid=1\r\n";
    sdp.append(ss.str());
    ss.str("");
    
    int width = 640;
    int height = 480;
    std::string profile = "64001f";
    std::string sps = "Z2QAH6zZQFAFuwEQACi7EAmJaAjxgjlg";
    std::string pps = "aOvjyyLA";
    
    server_->getVideoConfig(width, height, profile, sps, pps);
    
    ss << "a=rtpmap:96 H264/90000\r\na=mimetype:string;\"video/H264\"\r\na=framesize:96 " << width << "-" << height
       << "\r\na=Width:integer;" << width << "\r\na=Height:integer;" << height << "\r\n";
    sdp.append(ss.str());
    ss.str("");
    
    ss << "a=fmtp:96 packetization-mode=1;profile-level-id=" << profile << ";sprop-parameter-sets=" << sps << "," << pps << "\r\n";
    sdp.append(ss.str());
    ss.str("");
    
    return sdp;
}

std::string RtspClientConnection::createSession(int portRTP, int portRTCP)
{
    std::string session;
    
    //create Socket
    serverRtpFd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(serverRtpFd_ == -1){
        return session;
    }
    serverRtcpFd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(serverRtcpFd_ == -1){
        return session;
    }
    
    //Server Socket addr
    struct sockaddr_in addrServer;
    addrServer.sin_family = AF_INET;
    addrServer.sin_addr.s_addr = INADDR_ANY;
    addrServer.sin_port = htons(portRTP);

    if(bind(serverRtpFd_, (struct sockaddr*)&addrServer, sizeof(addrServer)) == -1){
        return session;
    }
    
    addrServer.sin_port = htons(portRTCP);
    if(bind(serverRtcpFd_, (struct sockaddr*)&addrServer, sizeof(addrServer)) == -1){
        return session;
    }

    ((struct sockaddr_in *)&connectAddr_)->sin_port = htons(portRTP);
    ((struct sockaddr_in *)&connectAddrRtcp_)->sin_port = htons(portRTCP);
    
    long sessionid = random();
    std::stringstream ss;
    ss << sessionid;
    session_ = ss.str();
    ssrc_ = random();
    timestamp_ = time(NULL);
    sequenceNumber_ = 10000;
    
    packets_ = 0;
    bytesSent_ = 0;
    rtpBase_ = 0;
    
    packetsReported_ = 0;
    bytesReported_ = 0;
    
    session = session_;
    
    /**
     init rtp header
     */
    memset(rtpHeader_, 0, 12);
    rtpHeader_[0] |= 0x80;          //version
    rtpHeader_[1] |= PayloadType;   //payload
    
    //ssrc
    rtpHeader_[8]  &= 0;
    rtpHeader_[9]  &= 0;
    rtpHeader_[10] &= 0;
    rtpHeader_[11] &= 0;
    rtpHeader_[8]  |= (char) ((ssrc_ & 0xff000000) >> 24);
    rtpHeader_[9]  |= (char) ((ssrc_ & 0x00ff0000) >> 16);
    rtpHeader_[10] |= (char) ((ssrc_ & 0x0000ff00) >> 8);
    rtpHeader_[11] |= (char) (ssrc_  & 0x000000ff);
    
    return session;
}

void RtspClientConnection::processCommand(char *buffer, int length)
{
    printf("%s\n", buffer);
    RtspMessage *message = new RtspMessage(buffer, length);
    if (message->requestCommand_ != "") {
        std::string command = message->requestCommand_;
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        std::string response;
        if (command == "options") {
            response = message->createResponse(200, "OK");
            response.append("Server: AVEncoderDemo/1.0\r\n");
            response.append("Public: DESCRIBE, SETUP, TEARDOWN, PLAY, OPTIONS\r\n\r\n");
        }
        else if (command == "describe") {
            response = message->createResponse(200, "OK");
            
            std::stringstream ss;
            ss << RtspServer::getIpAddress() << ":" << RtspServerPort;
            urlBase_ = ss.str();
            ss.str("");
            
            ss << "Content-base: rtsp://" << urlBase_ << "/\r\n";
            response.append(ss.str());
            ss.str("");
            
            std::string sdp = makeSDP();
            ss << "Content-Type: application/sdp\r\nContent-Length: " << sdp.size() << "\r\n\r\n";
            response.append(ss.str());
            ss.str("");
            
            response.append(sdp);
        }
        else if (command == "setup") {
            std::string transport = message->requests_["Transport:"];
            response = message->createResponse(451, "Need better error string here");

            if (transport.size()) {
                size_t pos = transport.find("client_port=");
                if (pos != std::string::npos) {
                    std::string portRange = transport.substr(pos + 12);
                    pos = portRange.find("-");
                    if (pos != std::string::npos) {
                        std::string stra = portRange.substr(0, pos);
                        std::string strb = portRange.substr(pos + 1);
                        int portRTP = atoi(stra.c_str());
                        int portRTCP = atoi(strb.c_str());
                        
                        std::string session_name = createSession(portRTP, portRTCP);
                        if (session_name.size() > 0)
                        {
                            response = message->createResponse(200, "OK");
                            std::stringstream ss;
                            ss << "Session: " << session_name << ";timeout=60\r\nTransport: RTP/AVP/UDP;unicast;client_port=" << portRTP << "-" << portRTCP
                               << ";server_port=" << portRTP << "-" << portRTCP << ";ssrc=" << ssrc_ <<";mode=play\r\nContent-Length: 0\r\n\r\n";
                            response.append(ss.str());
                            ss.str("");
                        }
                    }
                }
            }
        }
        else if (command == "play") {
            response = message->createResponse(200, "OK");
            std::stringstream ss;
            ss << "Session: " << session_ << "\r\n\r\n";
            response.append(ss.str());
            ss.str("");
            
            isSending_ = true;
        }
        else if (command == "teardown") {
            response = message->createResponse(200, "OK");
        }
        else if (command == "get_parameter") {
            
        }
        else {
            response = message->createResponse(451, "Method not recognised");
        }
        if (response.size() > 0) {
            int ret = (int)send(connectFd_, response.c_str(), response.size(), 0);
            printf("%s\n", response.c_str());
            assert(ret == response.size());
        }
    }
    delete message;
    message = nullptr;
}

void RtspClientConnection::onReceive(int &connectFd, char *buffer, int length, void *data)
{
    RtspClientConnection *pthis = (RtspClientConnection *)data;
    if (!pthis)
        return ;
    
    pthis->processCommand(buffer, length);
}

void RtspClientConnection::onReceiveRtcp(int &connectFd, char *buffer, int length, void *data)
{
    printf("rtcp %d\n", length);
}

void *RtspClientConnection::connectLoop(void *data)
{
    RtspClientConnection *pthis = (RtspClientConnection *)data;
    if (!pthis)
        return nullptr;
    
    pthis->recvBuffer = new char[BufferSize]();
    timeval timeout = {0, 200};
    fd_set rfd;
    int nfds;
    while (pthis->isRunning_) {
        FD_ZERO(&rfd);
        FD_SET(pthis->connectFd_, &rfd);
        nfds = select(pthis->connectFd_ + 1, &rfd, (fd_set *)0, (fd_set *)0, &timeout);
        if (nfds > 0) {
            if (FD_ISSET(pthis->connectFd_, &rfd)) {
                int ret = (int)recv(pthis->connectFd_, pthis->recvBuffer, BufferSize, 0);
                if (ret > 0) {
                    pthis->onReceive(pthis->connectFd_, pthis->recvBuffer, ret, data);
                }
                //  当使用 select()函数测试一个socket是否可读时，如果select()函数返回值为1，且使用recv()函数读取的数据长度为0 时，就说明该socket已经断开。
                else {
                    int err = errno;
                    if ((nfds == 1 && ret == 0 ) ||
                        err) {
                        pthis->server_->shutdownConnection(pthis->connectFd_);
                        break;
                    }
                }
            }
        }
        
        if (pthis->isSending_) {
            FD_ZERO(&rfd);
            FD_SET(pthis->serverRtcpFd_, &rfd);
            nfds = select(pthis->serverRtcpFd_ + 1, &rfd, (fd_set *)0, (fd_set *)0, &timeout);
            if (nfds > 0) {
                if (FD_ISSET(pthis->serverRtcpFd_, &rfd)) {
                    socklen_t len = sizeof(pthis->connectAddrRtcp_);
                    int ret = (int)recvfrom(pthis->serverRtcpFd_, pthis->recvBuffer, BufferSize, 0, (struct sockaddr *)&pthis->connectAddrRtcp_, &len);
                    if (ret > 0) {
                        pthis->onReceiveRtcp(pthis->serverRtcpFd_, pthis->recvBuffer, ret, data);
                    }
                }
            }
        }
    }

    return nullptr;
}

