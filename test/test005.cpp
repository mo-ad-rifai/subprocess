#include "subprocess.h"
namespace sp = subprocess;

int
main(int argc, char *argv[])
{
	// Start process
#ifdef _WIN32
	auto p = sp::Popen().Command("cmd /c echo Hello world!").Start()();
#else
	auto p = sp::Popen().Command("sh -c 'echo Hello world!'").Start()();
#endif
	// Do some computation in parallel
	int a[32][32], b[32][32], c[32][32];
	for (int row = 0; row < 32; ++row) {
		for (int col = 0; col < 32; ++col) {
			c[row][col] = 0;
			for (int k = 0; k < 32; ++k) {
				c[row][col] += a[row][k] * b[k][col];
			}
		}
	}
	// Wait for the process to return
	return p.Wait();
}
