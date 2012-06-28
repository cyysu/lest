#ifndef _REQUEST_H_
#define _REQUEST_H_

class CRequest
{
	public:
		CRequest();	
		~CRequest();
		void AddRequest(char *vBuffPtr, int vLen);
		inline int GetLength() { return mBuffLen; }
		inline char *GetData() { return mBuffPtr; }
	private:
		char *mBuffPtr;
		int mBuffLen;
};

#endif
