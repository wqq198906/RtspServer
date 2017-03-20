//
//  RtspMessage.cpp
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/14.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#include "RtspMessage.h"
#include <sstream>

RtspMessage::RtspMessage(char *data, int length)
{
    std::string message(data, length);
    //check is valid
    if (message.find("RTSP/1.0") != std::string::npos)
    {
        std::stringstream ss(message);
        std::string key, value;
        ss >> requestCommand_;
        while (ss >> key >> value) {
            requests_[key] = value;
        }
        if (requests_.size() >= 2) {
            std::string key = "CSeq:";
            requestSequence_ = requests_[key];
        }
    }
}

std::string RtspMessage::createResponse(int code, std::string desc)
{
    std::stringstream ss;
    ss << "RTSP/1.0 " << code << " " << desc << "\r\nCSeq: " << requestSequence_ << "\r\n";
    
    return ss.str();
}
