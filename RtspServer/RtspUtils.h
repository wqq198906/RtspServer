//
//  RtspUtils.h
//  RtspServer
//
//  Created by wangqiangqiang on 2016/12/14.
//  Copyright © 2016年 MOMO. All rights reserved.
//

#ifndef RtspUtils_h
#define RtspUtils_h

#include <string>

static const char* Base64Mapping = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const int max_packet_size = 1500;

void tonet_short(unsigned char* p, unsigned short s);
void tonet_long(unsigned char* p, unsigned long l);

std::string encodeLong(unsigned long val, int nPad);
std::string encodeToBase64(unsigned char* data, int length);

#endif /* RtspUtils_h */
