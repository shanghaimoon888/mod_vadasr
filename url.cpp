/*********************************************************************************
*    Copyright:  (C) 2019 zyjin
*                  All rights reserved.
*
*    Filename:  url.c
*    Description:  C语言实现URL编码
*
*    Created by zyjin on 19-7-18.
*
*    Version:  1.0.0(2019年7月18日)
*    Author:  zyjin <jzy2410723051@163.com><zzzzyjin@foxmail.com>
*    ChangeLog:  1, Release initial version on "2019年7月18日 22时43分"
*
********************************************************************************/
#include "url.h"
int hex2dec(char c)
{
	if ('0' <= c && c <= '9')
	{
		return c - '0';
	}
	else if ('a' <= c && c <= 'f')
	{
		return c - 'a' + 10;
	}
	else if ('A' <= c && c <= 'F')
	{
		return c - 'A' + 10;
	}
	else
	{
		return -1;
	}
}

char dec2hex(short int c)
{
	if (0 <= c && c <= 9)
	{
		return c + '0';
	}
	else if (10 <= c && c <= 15)
	{
		return c + 'A' - 10;
	}
	else
	{
		return -1;
	}
}
//编码一个url
void urlencode(char url[])
{
	int i = 0;
	int len = strlen(url);
	int res_len = 0;
	char res[BURSIZE];
	for (i = 0; i < len; ++i)
	{
		char c = url[i];
		if (('0' <= c && c <= '9') ||
			('a' <= c && c <= 'z') ||
			('A' <= c && c <= 'Z') ||
			c == '/' || c == '.' || c == '#' || c == '~' || c == '&'
			|| c == ':' || c == ';' || c == '=' || c == '?' || c == '@' ||
			c == '$' || c == '*' || c == '(' || c == ')' || c == ','
			)
		{
			res[res_len++] = c;
		}
		else
		{
			int j = (short int)c;
			if (j < 0)
				j += 256;
			int i1, i0;
			i1 = j / 16;
			i0 = j - i1 * 16;
			res[res_len++] = '%';
			res[res_len++] = dec2hex(i1);
			res[res_len++] = dec2hex(i0);
		}
	}
	res[res_len] = '\0';
	strcpy(url, res);
}

// 解码url
void urldecode(char url[])
{
	int i = 0;
	int len = strlen(url);
	int res_len = 0;
	char res[BURSIZE];
	for (i = 0; i < len; ++i)
	{
		char c = url[i];
		if (c != '%')
		{
			res[res_len++] = c;
		}
		else
		{
			char c1 = url[++i];
			char c0 = url[++i];
			int num = 0;
			num = hex2dec(c1) * 16 + hex2dec(c0);
			res[res_len++] = num;
		}
	}
	res[res_len] = '\0';
	strcpy(url, res);
}