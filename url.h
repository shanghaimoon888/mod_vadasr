/*********************************************************************************
*    Copyright:  (C) 2019 zyjin
*                  All rights reserved.
*
*    Filename:  url.h
*    Description:  C语言实现URL编码
*
*    Created by zyjin on 19-7-18.
*
*    Version:  1.0.0(2019年7月18日)
*    Author:  zyjin <jzy2410723051@163.com><zzzzyjin@foxmail.com>
*    ChangeLog:  1, Release initial version on "2019年7月18日 22时43分"
*
********************************************************************************/
#ifndef URL_190718_URL_H
#define URL_190718_URL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BURSIZE 480000
int hex2dec(char c);
char dec2hex(short int c);
void urlencode(char url[]);
void urldecode(char url[]);

#endif //URL_190718_URL_H
