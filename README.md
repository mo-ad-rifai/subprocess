# C++ subprocess
C++ implementation of Python's subprocess library.

Main features: `Popen`, `call`, `check_output`

Supported environments: Windows, POSIX.

#### Use Examples
```cpp
sp::Popen{{"cmd.exe /c echo Hello world!"}}.Wait();
```
```cpp
sp::Popen{{"cmd.exe", "/c", "echo", "Hello world!"}, true}.Wait();
```
```cpp
sp::Popen().Command("cmd.exe /c echo Hello world!").Wait();
```
```cpp
sp::Popen().Arguments({"cmd.exe", "/c", "echo", "Hello world!"}).Wait();
```
```cpp
auto p = sp::Popen().Command("cmd.exe /c echo Hello world!").Start()();
// ...
p.Wait();
```
```cpp
FILE* f = fopen("tmp.txt", "r");
sp::Popen().Arguments("a.exe").StdIn(f).Wait();
fclose(f);
// or alternatively
sp::Popen().Arguments("a.exe").StdIn({fopen("tmp.txt", "r"), true}).Wait();
```
```cpp
// Example with execution timeout
sp::Popen p;
// Ping localhost ten times
p.Command("ping -n 10 127.0.0.1").StdErr(sp::DEVNUL);
try {
	p.Wait(100);// 100 ms
} catch (const sp::TimeoutExpired&) {
	p.Terminate();
}
```

#### More Examples
```cpp
#include "subprocess.h"

std::string
rtrimmed(std::string s);

void
examples()
{
    namespace sp = subprocess;
    using namespace std::string_literals;
    {
	    // Run `ls' and print its output on the terminal.
        sp::Popen()
            .Arguments({"/bin/bash", "-c", "ls"})
            .Directory("/")
            .Wait();
    }
    {
	    // Run `bc' and give it input data through stdin.
        auto p = sp::Popen()
            .Arguments({"/usr/bin/bc"})
            .StdIn(sp::PIPE)
            .Communicate("2 + 3\n1"s);
    }
    {
	    // Run `bc' and give it the content of a file.
        auto p = sp::Popen()
            .Arguments({"/usr/bin/bc"})
            // StdIn, StdOut, StdErr can be: not set, FILE*, file descriptor, sp::PIPE, or sp::DEVNUL
            .StdIn({fopen("tmp.txt", "r"), true})// `true' requests the automatic closing of the file when the object if destroyed.
            .StdOut(sp::PIPE)
            .StdErr(sp::STDOUT)// Errors are redirected to stdout
            .Communicate();
        std::cout << "stdout: " << rtrimmed(p.output.string()) << std::endl;
        // if StdErr(sp::PIPE) is used, then
        // std::cout << "stderr: " << rtrimmed(p.error.string()) << std::endl;
    }
    {
        // Run `bc' and give it the content of a file. The output is redirected to a file and the errors are discarded.
        auto p = sp::Popen()
            .Arguments({"/usr/bin/bc"})
            .StdIn({fopen("tmp.txt", "r"), true})
            .StdOut({fopen("out.txt", "w"), true})
            .StdErr(sp::DEVNUL)
            .Communicate();
        std::cout << "out.txt: " << rtrimmed(sp::Pipe::Receiver({fopen("out.txt", "r"), true}).Receive().string()) << std::endl;
    }
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
```

#### Compilation Requirements
GCC 8 or newer

C++17 or newer

`-pthread`

#### References
[Python subprocess documentation](https://docs.python.org/2/library/subprocess.html)
