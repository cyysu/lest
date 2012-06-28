#include "httpheader.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

CHttpHeader::CHttpHeader()
{
	mMethod = NULL;
	mPath = NULL;
	mProtocol = NULL;
	mQuery = NULL;
	mContentLength = 0;
	mHttpData = NULL;
	mEMethod = METHOD_NONE;
}

CHttpHeader::~CHttpHeader()
{

}

int CHttpHeader::Parse(CRequest &vRequest)
{
	char *p = vRequest.GetData();
	mMethod = p;
	p = strpbrk(p, " \t\n\r");
	*p++ = '\0';
	p += strspn(p, " \t\n\r");

	mPath = p;
	p = strpbrk(p, " \t\n\r");
	char *tmp = mPath;
	tmp = strpbrk(tmp, "\?");
	if( tmp != NULL && tmp < p )
	{
		mQuery = tmp + 1;
		*tmp = '\0';
	}
	*p++ = '\0';
	p += strspn(p, " \t\n\r");

	mProtocol = p;
	p = strpbrk(p, " \t\n\r");
	if( *p == ' ' || *p == '\t' )
	{
		*p++ = '\0';
		while( *p == ' ' || *p == '\t' )
		{
			p++;	
		}

		if( *p != '\r' || *(p + 1) != '\n' )
		{
			return -1;
		}

		p += 2;
	}
	else if( *p == '\n' )
	{
		return -2;
	}
	else
	{
		if( *(p + 1) != '\n' )
		{
			return -3;
		}

		*p = '\0';
		p += 2;
	} // 第一行解析完毕

	// 获取其他属性
	char *endline;
	while( (endline = strstr(p, "\r\n")) != NULL && strstr(p, "\r\n\r\n") != NULL )
	{
		if( strncasecmp(p, "Content-Length:", 15) == 0 )
		{
			p += 15;
			p += strspn(p, " \t");
			tmp = strpbrk(p, " \t\n\r");
			*tmp = '\0';
			mContentLength = atoi(p);
		}
		p = endline + 2;
	}

	p += 2;

	// 其他数据
	mHttpData = p;	
	DecodePath();
	
	return 0;
}

void CHttpHeader::DecodePath()
{
	char *from = mPath;
	char *to = mPath;
	for( ; *from != '\0' ; ++to, ++from )
	{
		if( from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]) )
		{
			*to = Hexit(from[1]) * 16 + Hexit(from[2]);
			from += 2;
		}
		else
		{
			*to = *from;
		}
	}
	*to = '\0';	
}

int CHttpHeader::Hexit(char c)
{
	if( c >= '0' && c <= '9' )
	{
		return c - '0';
	}

	if( c >= 'a' && c <= 'f' )
	{
		return c - 'a' + 10;
	}

	if( c >= 'A' && c <= 'F' )
	{
		return c - 'A' + 10;
	}

	return 0;
}

EMethod CHttpHeader::ParseMethod()
{
	if( strcasecmp(mMethod, "get") == 0 )
	{
		mEMethod = METHOD_GET;
	}
	else if( strcasecmp(mMethod, "post") == 0 )
	{
		mEMethod = METHOD_POST;
	}
	else if( strcasecmp(mMethod, "head") == 0 )
	{
		mEMethod = METHOD_HEAD;
	}
	return mEMethod;
}
