//
//  RtspServer.h
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/13.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#ifndef RtspServer_h
#define RtspServer_h

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <pthread.h>
#include <string>
#include <netdb.h>
#include <ifaddrs.h>

#define BufferSize      1024
#define RtspServerPort  8554
#define RtpServerPort   50000
#define PayloadType     96

class RtspClientConnection;

class RtspServer
{
public:
    RtspServer();
    ~RtspServer();
    
    void                setVideoConfig(char *config, int configLen);
    void                getVideoConfig(int &width, int &height, std::string &profile, std::string &sps, std::string &pps);
    
    void                startupServer();
    void                shutdownServer();
    
    void                shutdownConnection(int connectFd);
    void                onVideoData(char *data, int length, double pts, bool isKey);
    
    static std::string  getIpAddress();
    
protected:
    void                cleanup();
    static void        *serverLoop(void *data);
    static void         onAccept(int &clientFd, sockaddr_in &clientAddr, void *data);
    
private:
    int                                         serverFd_;
    bool                                        isRunning_;
    pthread_t                                   serverTid_;
    pthread_mutex_t                             connectsLock_;
    pthread_mutex_t                             closeConnectsLock_;
    std::map<int, RtspClientConnection*>        connects_;
    std::vector<int>                            closeConnects_;
    
    //video config
    char                                       *videoConfig_;
    int                                         videoConfigLength_;
};



#endif /* RtspServer_h */
