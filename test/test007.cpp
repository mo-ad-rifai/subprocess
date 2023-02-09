#include <cstdio>
#include "subprocess.h"
namespace sp = subprocess;

int
main(int argc, char *argv[])
{
	return sp::Popen()
		.Arguments({"python", "-c", "print('%.5f' % eval(input()))"})
		.StdIn({fopen("test007.ref.in", "r"), true})
		.Wait();
}
