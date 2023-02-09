#include <cstdio>
#include "subprocess.h"
namespace sp = subprocess;

std::string
rtrimmed(std::string s);

int
main(int argc, char *argv[])
{
	// Run `python' and give it the content of a file.
	auto r = sp::Popen()
		.Arguments({"python", "-c", "print('%.5f' % input())"})
		// StdIn, StdOut, StdErr can be: not set, FILE*, file descriptor, sp::PIPE, or sp::DEVNUL
		.StdIn({fopen("test011.ref.in", "r"), true})// `true' requests the automatic closing of the file when the object if destroyed.
		.StdOut(sp::PIPE)
		.StdErr(sp::STDOUT)// Errors are redirected to stdout
		.Communicate();
	if ("3.14159" == rtrimmed(r.output.string())) return 0;
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
