#include "subprocess.h"
namespace sp = subprocess;

int
main(int argc, char *argv[])
{
#ifdef _WIN32
	return sp::Popen().Command("cmd /c echo Hello world!").Wait();
#else
	return sp::Popen().Command("sh -c 'echo Hello world!'").Wait();
#endif
}
