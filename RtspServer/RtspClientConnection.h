//
//  RtspClientConnection.h
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/14.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#ifndef RtspClientConnection_h
#define RtspClientConnection_h

#include "RtspServer.h"

class RtspClientConnection
{
public:
    RtspClientConnection(int connectFd, sockaddr_in connectAddr, RtspServer *server);
    
    void            onVideoData(char *data, int length, double pts, bool isKey);
    void            shutdown();
    
protected:
    static void     *connectLoop(void *data);
    static void     onReceive(int &connectFd, char *buffer, int length, void *data);
    static void     onReceiveRtcp(int &connectFd, char *buffer, int length, void *data);
    void            processCommand(char *buffer, int length);
    std::string     makeSDP();
    std::string     createSession(int portRTP, int portRTCP);
    
    void            createAndSendRtpPacket(char *data, int length);
    void            setRtpPacketSequenceNumber(int number);
    void            setRtpPacketTimestamp(unsigned int timestamp);
    void            setRtpPacketMarker(int marker);
    void            setFUIndicator(char *data);
    void            setFUHeader(char *data, bool start, bool end);
    
private:
    std::string     urlBase_;
    int             connectFd_;
    sockaddr_in     connectAddr_;
    sockaddr_in     connectAddrRtcp_;
    RtspServer     *server_;
    pthread_t       connectTid_;
    bool            isRunning_;
    char           *recvBuffer;
    
    int             serverRtpFd_;
    int             serverRtcpFd_;
    bool            isSending_;
    unsigned char   rtpHeader_[12];
    char            fuIndicator_;
    char            fuHeader_;
    
    std::string     session_;
    long            ssrc_;
    long            timestamp_;
    int             sequenceNumber_;
    long            packets_;
    long            bytesSent_;
    bool            bFirst_;
    
    // time mapping using NTP
    uint64_t        ntpBase_;
    uint64_t        rtpBase_;
    double          ptsBase_;
    
    // RTCP stats
    long            packetsReported_;
    long            bytesReported_;
};

#endif /* RtspClientConnection_h */
