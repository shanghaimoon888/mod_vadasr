/*********************************************************************************
*    Copyright:  (C) 2019 zyjin
*                  All rights reserved.
*
*    Filename:  url.h
*    Description:  C����ʵ��URL����
*
*    Created by zyjin on 19-7-18.
*
*    Version:  1.0.0(2019��7��18��)
*    Author:  zyjin <jzy2410723051@163.com><zzzzyjin@foxmail.com>
*    ChangeLog:  1, Release initial version on "2019��7��18�� 22ʱ43��"
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
