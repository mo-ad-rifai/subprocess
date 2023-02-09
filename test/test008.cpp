#include "subprocess.h"
namespace sp = subprocess;

int
main(int argc, char *argv[])
{
	// Ping localhost ten times
	sp::Popen p;
#ifdef _WIN32
	p.Command("ping /n 10 127.0.0.1").StdErr(sp::DEVNUL);
#else
	p.Command("ping -c 10 127.0.0.1").StdErr(sp::DEVNUL);
#endif
	try {
		p.Wait(100);// 100 ms
	} catch (const sp::TimeoutExpired&) {
		p.Terminate();
		return 0;
	}
	return 1;
}
