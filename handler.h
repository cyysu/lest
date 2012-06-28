#ifndef _HANDLER_H_
#define _HANDLER_H_

#include "request.h"
#include "httpheader.h"

class CHandler
{
	public:
		CHandler(int vSocketFD);
		~CHandler();
		static void Handler(int vSocketFD);
	private:
		void Close();
		int RecvMsg();
		int RecvMsg(char *vBuffer, int vLen);
		void SendMsg(CRequest &vRequest);
		void SendMsg(char *vBuffPtr, int vLeftLen);
		void SetNonBlock();
		static void ReadTimeout(int vSigNum);
		void SendError(int vErrorNum, const char *vTitle, const char *vText);
		char *ChooseType(char *vFileName);
		void DoCGI(char *vFileName);
		void DoFile(char *vFileName);
		void DoDir(char *vDirName);
	private:
		static CHandler *mHandler;
		int mSocketFD;
		CRequest mRequest;
		CHttpHeader mHttpHeader;
};

#endif
