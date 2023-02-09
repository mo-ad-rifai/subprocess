#include "subprocess.h"
namespace sp = subprocess;

std::string
rtrimmed(std::string s);

int
main(int argc, char *argv[])
{
	using namespace std::string_literals;
	// Run `python' and give it a mathematical expression through stdin.
	auto r = sp::Popen()
		.Arguments({"python", "-c", "print('%.5f' % input())"})
		.StdIn(sp::PIPE)
		.Communicate("355.0 / 113.0"s);
	return 0;
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
