/*
 * This process is spawned from posix.dll.
 * Running in any other context should exit non-0.
 * There be magic in posix.dll that makes this work.
 */

#include <unistd.h>
#include <signal.h>

int main(int argc, char** argv)
{
	signal(SIGCHLD, SIG_IGN);
	for (;;)
	{
		sleep(5);
		pause();
	}
	return 1;
}
