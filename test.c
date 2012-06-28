#include <unistd.h>

int main(int argc, char *argv[])
{
	printf("%d\n", execlp("/home/guzheng/lest/cgi/test.pl", ""))
}
