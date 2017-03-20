//
//  RtspMessage.h
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/14.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#ifndef RtspMessage_h
#define RtspMessage_h

#include <string>
#include <map>

class RtspMessage
{
public:
    RtspMessage(char *data, int length);
    
    std::string createResponse(int code, std::string desc);
    
public:
    std::string                             requestCommand_;
    std::string                             requestSequence_;
    std::map<std::string, std::string>      requests_;
};

#endif /* RtspMessage_h */
