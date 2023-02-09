#include "subprocess.h"
namespace sp = subprocess;

int
main(int argc, char *argv[])
{
#ifdef _WIN32
	return sp::Popen{{"cmd", "/c", "echo Hello world!& >&2 echo Bad behavior"}, true}.Wait();
#else
	return sp::Popen{{"sh",  "-c", "echo Hello world!; >&2 echo Bad behavior"}, true}.Wait();
#endif
}
