#include <cstdio>
#include "subprocess.h"
namespace sp = subprocess;

int
main(int argc, char *argv[])
{
	FILE* f = fopen("test006.ref.in", "r");
	auto ret = sp::Popen().Arguments({"python", "-c", "print('%.5f' % input())"}).StdIn(f).Wait();
	fclose(f);
	return ret;
}
