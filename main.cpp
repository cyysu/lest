#include "listener.h"
#include "handler.h"
#include <signal.h>
#include <sys/wait.h>

void sigchld_hander(int signum)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
	signal(SIGCHLD, sigchld_hander);
}

int main(int argc, char *argv[])
{
	signal(SIGCHLD, sigchld_hander);
	CListener listener = CListener(CHandler::Handler);
	int ret = listener.Listen(9999);
	if( ret < 0 )
	{
		return 1;
	}
	
	listener.Run();
	
	return 0;
}
