#include <cstdio>
#include "subprocess.h"
namespace sp = subprocess;

std::string
rtrimmed(std::string s);

int
main(int argc, char *argv[])
{
	// Run `python' and give it the content of a file. The output is redirected to a file and the errors are discarded.
	auto r = sp::Popen()
		.Arguments({"python", "-c", "print('%.5f' % input())"})
		.StdIn({fopen("test012.ref.in", "r"), true})
		.StdOut({fopen("test012.out", "w"), true})
		.StdErr(sp::DEVNUL)
		.Communicate();
	// Read the output file and check for correctness of the result
	if ("3.14159" == rtrimmed(sp::Pipe::Receiver({fopen("test012.out", "r"), true}).Receive().string())) return 0;
	return 1;
}


// Helper functions for removing whitespace from the end of a string

#include <algorithm>

void
rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int c) {
        return !std::isspace(c);
    }).base(), s.end());
}

std::string
rtrimmed(std::string s) {
    rtrim(s);
    return s;
}
