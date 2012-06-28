#include "listener.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

CListener::CListener(AcceptHandler mHandler)
{
	mSocketFD = -1;
	mAcceptHandler = mHandler;
}

CListener::~CListener()
{
	Close();
}

int CListener::Listen(unsigned short vPort)
{
	Close();

	mSocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if( mSocketFD < 0 )
	{
		printf("socket create failed: %s\n", strerror(errno));
		return -1;
	}

	int reuseflag = 1;
	if( setsockopt(mSocketFD, SOL_SOCKET, SO_REUSEADDR, &reuseflag, sizeof(reuseflag)) < 0 )
	{
		printf("set reuse address failed: %s\n", strerror(errno));
		Close();
		return -2;
	}

	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	sockaddr.sin_port = htons(vPort);
	
	if( bind(mSocketFD, (const struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0 )
	{
		printf("bind ip and address failed: %s\n", strerror(errno));
		Close();
		return -3;
	}

	if( listen(mSocketFD, 1024) < 0 )
	{
		printf("listen failed: %s\n", strerror(errno));
		Close();
		return -4;
	}

	printf("listening on port %d\n", vPort);
	return 0;
}

void CListener::Run()
{
	while(1)
	{
		fd_set fds;	
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		
		FD_ZERO(&fds);
		FD_SET(mSocketFD, &fds);
		int ret = select(mSocketFD + 1, &fds, NULL, NULL, &tv);
		if( ret == 0 )
		{
			usleep(1);
			continue;
		}
		else if( ret < 0 )
		{
			continue;
		}
		else
		{
			if( FD_ISSET(mSocketFD, &fds) )
			{
				struct sockaddr_in sockaddr;
				socklen_t socklen = sizeof(sockaddr);
				ret = accept(mSocketFD, (struct sockaddr *)&sockaddr, &socklen);
				if( ret < 0 )
				{
					printf("accept failed: %s\n", strerror(errno));
					continue;
				}

				printf("receive a connection fd(%d) from %s:%d\n", ret, inet_ntoa(sockaddr.sin_addr), ntohs(sockaddr.sin_port));
				mAcceptHandler(ret);
			}
		}
	}
}

void CListener::Close()
{
	if( mSocketFD >= 0 )
	{
		close(mSocketFD);
		mSocketFD = -1;
	}
}
