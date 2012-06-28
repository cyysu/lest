#include "request.h"
#include <stdlib.h>
#include <string.h>

CRequest::CRequest()
{
	mBuffPtr = NULL;	
	mBuffLen = 0;
}

CRequest::~CRequest()
{
	if( mBuffPtr != NULL )
	{
		free(mBuffPtr);
	}
}

void CRequest::AddRequest(char *vBuffPtr, int vLen)
{
	if( mBuffPtr == NULL )
	{
		mBuffPtr = (char *)malloc(vLen + 1);
		memcpy(mBuffPtr, vBuffPtr, vLen);
		mBuffPtr[vLen] = '\0';
		mBuffLen = vLen;
	}
	else
	{
		mBuffPtr = (char *)realloc(mBuffPtr, vLen + mBuffLen + 1);
		memcpy(mBuffPtr + mBuffLen, vBuffPtr, vLen);
		mBuffPtr[vLen + mBuffLen] = '\0';
		mBuffLen += vLen;
	}
}
