//
//  RtspServer.cpp
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/13.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#include "RtspServer.h"
#include "RtspClientConnection.h"
#include "NALUnit.h"
#include "RtspUtils.h"

std::string RtspServer::getIpAddress()
{
    std::string ipstr;
//    char hostname[BufferSize] = {0};
//    gethostname(hostname, BufferSize);
//    hostent *phost = gethostbyname(hostname);
//    if (phost) {
//        ipstr = inet_ntoa(*((struct in_addr *)phost->h_addr_list[0]));
//    }

    struct ifaddrs * ifAddrStruct = NULL;
    void * tmpAddrPtr = NULL;
    
    getifaddrs(&ifAddrStruct);
    while (ifAddrStruct != NULL) {
        if (ifAddrStruct->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            std::string ip = addressBuffer;
            if (ip != "127.0.0.1") {
                ipstr = ip;
                break;
            }
            // printf("%s IP Address %s\n", ifAddrStruct->ifa_name, addressBuffer);
        }
        else if (ifAddrStruct->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            // printf("%s IP Address %s\n", ifAddrStruct->ifa_name, addressBuffer);
        }
        ifAddrStruct=ifAddrStruct->ifa_next;
    }
    
    return ipstr;
}

RtspServer::RtspServer()
{
    isRunning_ = false;
    pthread_mutex_init(&connectsLock_, nullptr);
    pthread_mutex_init(&closeConnectsLock_, nullptr);
    videoConfig_ = NULL;
    videoConfigLength_ = 0;
}

RtspServer::~RtspServer()
{
    cleanup();
}

void RtspServer::cleanup()
{
    if (isRunning_) {
        isRunning_ = false;
        pthread_join(serverTid_, nullptr);
        pthread_mutex_lock(&connectsLock_);
        std::map<int, RtspClientConnection*>::iterator it = connects_.begin();
        while (it != connects_.end()) {
            it->second->shutdown();
            delete it->second;
            it->second = nullptr;
            ++it;
        }
        connects_.clear();
        std::map<int, RtspClientConnection*>().swap(connects_);
        pthread_mutex_unlock(&connectsLock_);
        
        pthread_mutex_destroy(&connectsLock_);
        pthread_mutex_destroy(&closeConnectsLock_);
    }
}

//00 00 00 01 sps 00 00 00 01 pps
void RtspServer::setVideoConfig(char *config, int configLen)
{
    delete []videoConfig_;
    videoConfig_ = nullptr;
    videoConfigLength_ = configLen;
    videoConfig_ = new char[videoConfigLength_]();
    memcpy(videoConfig_, config, videoConfigLength_);
}

void RtspServer::getVideoConfig(int &width, int &height, std::string &profile, std::string &sps, std::string &pps)
{
    char *pdata = videoConfig_;
    int length = videoConfigLength_;
    int frameLength = 0;
    char *frameStartPtr = NULL;
    NALUnit *spsNal = nullptr;
    NALUnit *ppsNal = nullptr;
    
    for(int i = 0; i < length; i++) {
        //H.264 StartCode 00 00 00 01
        if(*((int*)(pdata + i)) == 0x01000000) {
            if(frameLength > 0) {
                spsNal = new NALUnit((unsigned char *)(frameStartPtr + 4), frameLength - 4);
                
                SeqParamSet seqParams;
                seqParams.Parse(spsNal);
                width = (int)seqParams.EncodedWidth();
                height = (int)seqParams.EncodedHeight();
                char buf[10] = {0};
                sprintf(buf, "%02x%02x%02x", seqParams.Profile(), seqParams.Compat(), seqParams.Level());
                profile = buf;
                sps = encodeToBase64((unsigned char *)spsNal->Start(), spsNal->Length());
                
                delete spsNal;
                spsNal = nullptr;
            }
            frameStartPtr = pdata + i;
            frameLength = 0;
            i++;//StartCode(0x00010000)
            frameLength++;
        }
        frameLength++;
    }
    //if only one startcode
    if(frameLength > 0) {
        ppsNal = new NALUnit((unsigned char *)(frameStartPtr + 4), frameLength - 4);
        pps = encodeToBase64((unsigned char *)ppsNal->Start(), ppsNal->Length());
        
        delete ppsNal;
        ppsNal = nullptr;
    }
}

void RtspServer::startupServer()
{
    sockaddr_in addrServer;
    
    //Create Socket
    serverFd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //Server Socket IP
    addrServer.sin_family = AF_INET;
    addrServer.sin_addr.s_addr = INADDR_ANY;
    addrServer.sin_port = htons(RtspServerPort);
    
    //reuse
    int option = 1;
    if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
        printf("startupServer reuse failed!\n");
        close(serverFd_);
        return;
    }
    
    //Bind Socket
    if(bind(serverFd_, (sockaddr*)&addrServer, sizeof(addrServer)) == -1){
        printf("startupServer bind failed!\n");
        close(serverFd_);
        return;
    }
    //listen
    if(listen(serverFd_, 10) == -1){
        printf("startupServer listen failed!\n");
        close(serverFd_);
        return;
    }
    
    isRunning_ = true;
    int res = pthread_create(&serverTid_, NULL, serverLoop, this);
    if (res != 0) {
        printf("startupServer thread create failed.\n");
    }
}

void RtspServer::shutdownServer()
{
    cleanup();
}

void RtspServer::shutdownConnection(int connectFd)
{
    pthread_mutex_lock(&closeConnectsLock_);
    closeConnects_.push_back(connectFd);
    pthread_mutex_unlock(&closeConnectsLock_);
}

void RtspServer::onVideoData(char *data, int length, double pts, bool isKey)
{
    pthread_mutex_lock(&connectsLock_);
    std::map<int, RtspClientConnection*>::iterator it = connects_.begin();
    while (it != connects_.end()) {
        it->second->onVideoData(data, length, pts, isKey);
        ++it;
    }
    pthread_mutex_unlock(&connectsLock_);
}

void RtspServer::onAccept(int &clientFd, sockaddr_in &clientAddr, void *data)
{
    RtspServer *pthis = (RtspServer *)data;
    if (!pthis)
        return ;
    
    RtspClientConnection *connect = new RtspClientConnection(clientFd, clientAddr, pthis);
    pthread_mutex_lock(&pthis->connectsLock_);
    pthis->connects_[clientFd] = connect;
    pthread_mutex_unlock(&pthis->connectsLock_);
}

void *RtspServer::serverLoop(void *data)
{
    RtspServer *pthis = (RtspServer *)data;
    if (!pthis)
        return nullptr;

    timeval timeout = {0, 200};
    fd_set rfd;
    int nfds;
    while (pthis->isRunning_) {
        
        //close connection
        {
            std::vector<int> closeConnects;
            pthread_mutex_lock(&pthis->closeConnectsLock_);
            closeConnects.swap(pthis->closeConnects_);
            pthread_mutex_unlock(&pthis->closeConnectsLock_);
            
            pthread_mutex_lock(&pthis->connectsLock_);
            for (int i = 0; i < closeConnects.size(); ++i) {
                std::map<int, RtspClientConnection*>::iterator it = pthis->connects_.begin();
                while (it != pthis->connects_.end()) {
                    if (it->first == closeConnects[i]) {
                        it->second->shutdown();
                        delete it->second;
                        it->second = nullptr;
                        pthis->connects_.erase(it);
                        break;
                    }
                    ++it;
                }
            }
            pthread_mutex_unlock(&pthis->connectsLock_);
        }
        
        FD_ZERO(&rfd);
        FD_SET(pthis->serverFd_, &rfd);
        nfds = select(pthis->serverFd_ + 1, &rfd, (fd_set *)0, (fd_set *)0, &timeout);
        if (nfds == 0) {
            continue;
        }
        else if (nfds > 0) {
            FD_CLR(pthis->serverFd_, &rfd);
            sockaddr_in addrClient;
            socklen_t addrClientLen = sizeof(addrClient);
            int clientFd = accept(pthis->serverFd_, (struct sockaddr *)&addrClient, &addrClientLen);
            if (clientFd > 0) {
                pthis->onAccept(clientFd, addrClient, data);
            }
        }
    }
    return nullptr;
}
