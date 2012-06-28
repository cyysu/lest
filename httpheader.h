#ifndef _HTTP_HEADER_H_
#define	_HTTP_HEADER_H_

#include "request.h"

enum EMethod
{
	METHOD_NONE = 0,
	METHOD_GET,
	METHOD_HEAD,
	METHOD_POST
};

class CHttpHeader
{
	public:
		CHttpHeader();
		~CHttpHeader();
		int Parse(CRequest &vRequest);
		inline EMethod GetMethod() { return mEMethod; }
		inline char *GetPath() { return mPath; }
		inline char *GetProtocol() { return mProtocol; }
		inline char *GetQuery() { return mQuery; }
		int GetContentLength() { return mContentLength; }
		char *GetHttpData() { return mHttpData; }
		EMethod ParseMethod();
	private:
		void DecodePath();
		int Hexit(char c);
	private:
		char *mMethod;
		char *mPath;
		char *mProtocol;
		char *mQuery;
		int mContentLength;
		char *mHttpData;
		EMethod mEMethod;
};

#endif
