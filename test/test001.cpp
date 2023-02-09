#include "subprocess.h"
namespace sp = subprocess;

int
main(int argc, char *argv[])
{
#ifdef _WIN32
	return sp::Popen{{"cmd /c echo Hello world!"}}.Wait();
#else
	return sp::Popen{{"sh -c 'echo Hello world!'"}}.Wait();
#endif
}
