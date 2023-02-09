#include "subprocess.h"
namespace sp = subprocess;

std::string
rtrimmed(std::string s);

int
main(int argc, char *argv[])
{
	// Set the work directory as the file system's root and check for success by looking for a directory that usually exists there and not in the executable's parent directory
	if (
#ifdef _WIN32
	"Windows" == rtrimmed(sp::check_output({"cmd", "/c", "dir /B | findstr /L /X Windows"}, sp::INFINITE_TIME, {}, {}, "C:\\").string())
#else
	"bin" == rtrimmed(sp::check_output({"sh", "-c", "ls -d bin"}, sp::INFINITE_TIME, {}, {}, "/").string())
#endif
	) return 0;
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
