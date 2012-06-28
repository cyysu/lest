#ifndef _LISTENER_H_
#define _LISTENER_H_

#include <string>
using namespace std;

typedef void (*AcceptHandler)(int vAcceptFD);

class CListener
{
	public:
		CListener(AcceptHandler vHandler);
		~CListener();
		int Listen(unsigned short vPort);
		void Run();
		void Close();
	private:
		int mSocketFD;
		AcceptHandler mAcceptHandler;
};

#endif
