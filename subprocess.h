#ifndef SUBPROCESS_H
#define SUBPROCESS_H

#include <iostream>
#include <exception>
#include <cstring>
#include <vector>
#include <chrono>
#include <future>
#ifdef _WIN32
#   include <windows.h>
#   include <io.h>
#else
#   include <spawn.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <sys/wait.h>
#   include <signal.h>

#   if STDIN_FILENO != 0 || STDOUT_FILENO != 1 || STDERR_FILENO != 2
#       error
#   endif
#endif

namespace subprocess {

using clock = std::chrono::steady_clock;
typedef unsigned char byte;
typedef unsigned long duration;
#ifdef _WIN32
typedef HANDLE file_id;
typedef HANDLE process_id;
typedef DWORD retcode;
#else
typedef int file_id;
typedef pid_t process_id;
typedef int retcode;
#endif

/// Exceptions

class Exception : public std::exception
{
private:
    std::string _msg;

public:
    Exception(const std::string& msg)
    :   _msg(msg)
    {}

    Exception(const char* msg)
    : _msg(msg)
    {}

    Exception(std::nullptr_t)
    {}

    const char*
    what() const noexcept
    { return _msg.c_str(); }
};

template<class E>
typename std::enable_if<std::is_base_of<std::exception, E>::value, bool>::type
_throw(const E& e) noexcept(false)
{ throw e; }

bool
_throw(const Exception& e) noexcept(false)
{ throw e; }

///

class Bytes : public std::basic_string<byte>
{
public:
    using std::basic_string<byte>::basic_string;

    Bytes(const std::string& s)
    :   std::basic_string<byte>(s.cbegin(), s.cend())
    {}

    operator std::string()
    { return string(); }

    std::string
    string() const
    { return {cbegin(), cend()}; }

    Bytes
    bytes() const
    { return *this; }
};

struct Return
{
    Bytes output, error;

    template<class T1, class T2>
    void
    Export(T1& o, T2& e)
    {
        o = std::move(output);
        e = std::move(error);
    }
};

#ifdef _WIN32
class OSError : public std::runtime_error
{
public:
    OSError(const std::string& function_name)
    :   std::runtime_error(nullptr)
    {
        char* message;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&message), 0, NULL);
        auto s = function_name + ": " + message;
        message = new char[s.size() + 1];
        memcpy(message, s.c_str(), s.size() + 1);
    }
};

void
_deleteString(std::string* s)
{ delete s; }

template<class T>
void
_noopDeleter(T* s)
{}

std::string
shellEscaped(const std::string& arg)
{
    std::string s;
    bool needquote = arg.empty() or arg.find_first_of(" \t") != std::string::npos;
    if (needquote) {
        s.reserve(arg.size() + 2);
        s += '"';
    } else {
        s.reserve(arg.size());
    }
    size_t k = 0;
    for (const auto& c : arg) {
        if (c == '\\') {
            ++k;
        } else if (c == '"') {
            s += std::string(2 * k + 2, '\\');
            s.back() = '"';
            k = 0;
        } else if (k > 0) {
            s += std::string(k + 1, '\\');
            s.back() = c;
            k = 0;
        } else {
            s += c;
        }
    }
    s += std::string(k, '\\');
    if (needquote) {
        s += std::string(k, '\\') + '"';
    }
    return s;
}
#else
class OSError : public std::runtime_error
{
public:
    OSError(const std::string& function_name)
    :   std::runtime_error(function_name + ": " + std::strerror(errno))
    {}

    OSError(const std::string& function_name, int errno_)
    :   std::runtime_error(function_name + ": " + std::strerror(errno_))
    {}
};

void
_deleteTwoArrays(char** a)
{
    delete[] a[0];
    delete[] a;
}

void
_deleteOneArray(char** a)
{ delete[] a; }

/// \todo add support for quotes, comments and other stuff
std::unique_ptr<char*[], decltype (&_deleteTwoArrays)>
shellSplitted(const char* str)
{
    // assuming str != nullptr
    auto ss = strlen(str) + 1;
    // copy str while substituting whitespace
    auto s = new char[ss];
    bool substituted = false;
    auto vs = 1u;
    auto e = s + ss;
    for (auto p = s; p != e; ++p, ++str) {
        if (*str == ' ' or *str == '\t') {
            *p = '\0';
            substituted = true;
            continue;
        }
        if (substituted) {
            substituted = false;
            ++vs;
        }
        *p = *str;
    }
    // record the positions of the strings
    auto r = new char*[vs + 1];
    auto p = r;
    *p = s;
    ++p;
    --e;
    for (++s; s != e; ++s) {
        if (s[-1] == '\0') {
            *p = s;
            ++p;
        }
    }
    *p = nullptr;
    return {r, _deleteTwoArrays};
}
#endif

class FileHandler
{
protected:
    union
    {
        file_id _id;
        FILE* _file;
#ifdef _WIN32
        int _fd;
#endif
    };
    enum {
        tNone,
        tFileId,
        tFILE,
        tFileDescriptor
    } _type;
    unsigned _self_closing;

public:
#ifdef _WIN32
    FileHandler()
    :   _id(INVALID_HANDLE_VALUE)
    ,   _type(tNone)
    ,   _self_closing(false)
    {}
#else
    FileHandler()
    :   _id(-1)
    ,   _type(tNone)
    ,   _self_closing(false)
    {}
#endif
    FileHandler(file_id id, bool self_closing = false)
    :   _id(id)
    ,   _type(tFileId)
    ,   _self_closing(self_closing)
    {}

    FileHandler(FILE* file, bool self_closing = false)
    :   _file(file)
    ,   _type(tFILE)
    ,   _self_closing(self_closing)
    {}
#ifdef _WIN32
    FileHandler(int fd, bool self_closing = false)
    :   _fd(fd)
    ,   _type(tFileDescriptor)
    ,   _self_closing(self_closing)
    {}
#endif
    FileHandler(FileHandler&) = delete;

    FileHandler(FileHandler&& o)
    {
        _id = o._id;
        _type = o._type;
        _self_closing = o._self_closing;
        // invalidate the other file handler
#ifdef _WIN32
        o._id = INVALID_HANDLE_VALUE;
#else
        o._id = -1;
#endif
        o._type = tNone;
        o._self_closing = false;
    }

    ~FileHandler()
    {
        if (IsSelfClosing()) {
            Close();
        }
    }

#ifdef _WIN32
    file_id
    Id() const
    {
        switch (_type) {
            case tFileId: return _id;
            case tFILE: return reinterpret_cast<file_id>(_get_osfhandle(fileno(_file)));
            case tFileDescriptor: return reinterpret_cast<file_id>(_get_osfhandle(_fd));
            default: return INVALID_HANDLE_VALUE;
        }
    }
#else
    file_id
    Id() const
    {
        switch (_type) {
            case tFileId: return _id;
            case tFILE: return fileno(_file);
            case tFileDescriptor: return _id;
            default: return -1;
        }
    }
#endif
    void
    Id(file_id id)
    {
        _id = id;
        _type = tFileId;
    }

    bool
    IsSelfClosing() const
    { return _self_closing; }

    void
    IsSelfClosing(bool self_closing)
    { _self_closing = self_closing; }

    bool
    IsFileId() const
    { return _type == tFileId; }

    bool
    IsFILE() const
    { return _type == tFILE; }
#ifdef _WIN32
    bool
    IsFileDescriptor() const
    { return _type == tFileDescriptor; }
#else
    bool
    IsFileDescriptor() const
    { return IsFileId(); }
#endif
#ifdef _WIN32
    bool
    IsValid() const
    { return (IsFileId() and _id != INVALID_HANDLE_VALUE) or (IsFILE() and _file != nullptr) or (IsFileDescriptor() and _fd != -1); }
#else
    bool
    IsValid() const
    { return (IsFileId() and _id != -1) or (IsFILE() and _file != nullptr); }
#endif
#ifdef _WIN32
    int
    Close() const
    {
        switch (_type) {
            case tFileId: return CloseHandle(_id);
            case tFILE: return fclose(_file);
            case tFileDescriptor: return _close(_fd);
            default: return -1;
        }
    }
#else
    int
    Close() const
    {
        switch (_type) {
            case tFileId: return close(_id);
            case tFILE: return fclose(_file);
            case tFileDescriptor: return close(_id);
            default: return -1;
        }
    }
#endif
#ifdef _WIN32
    FileHandler&
    MakeInheritable(bool inherit)
    {
        if (_type == tFileId) {
            SetHandleInformation(_id, HANDLE_FLAG_INHERIT, inherit)
            or _throw(OSError("SetHandleInformation"));
        }
        return *this;
    }
#else
    FileHandler&
    CloseOnExec(bool close)
    {
        int flags = fcntl(Id(), F_GETFD, 0);
        if (close) {
            flags |= FD_CLOEXEC;
        } else {
            flags &= ~FD_CLOEXEC;
        }
        fcntl(Id(), F_SETFD, flags) == 0 or _throw(OSError("fcntl(2)"));
        return *this;
    }
#endif
};

namespace Pipe
{
    class Receiver : public FileHandler
    {
    public:
        using FileHandler::FileHandler;
#ifdef _WIN32
        ssize_t
        Receive(void* buf, size_t count) const
        {
            if (IsFileId()) {
                DWORD size, length = 0;
                while (count > 0 and ReadFile(_id, buf, count, &size, NULL) and size > 0) {
                    length += size;
                    count -= static_cast<size_t>(size);
                    buf = static_cast<char*>(buf) + size;
                }
                return length;
            } else if (IsFILE()) {
                int size, length = 0;
                while (count > 0 and (size = fread(buf, 1, count, _file)) > 0) {
                    length += size;
                    count -= static_cast<size_t>(size);
                    buf = static_cast<char*>(buf) + size;
                }
                return length;
            } else if (IsFileDescriptor()) {
                int size, length = 0;
                while (count > 0 and (size = _read(_fd, buf, count)) > 0) {
                    length += size;
                    count -= static_cast<size_t>(size);
                    buf = static_cast<char*>(buf) + size;
                }
                return length;
            }
            return -1;
        }
#else
        ssize_t
        Receive(void* buf, size_t count) const
        {
            if (IsFileId()) {
                ssize_t size, length = 0;
                while (count > 0 and (size = read(_id, buf, count)) > 0) {
                    length += size;
                    count -= static_cast<size_t>(size);
                    buf = static_cast<char*>(buf) + size;
                }
                return length;
            } else if (IsFILE()) {
                size_t size, length = 0;
                while (count > 0 and (size = fread(buf, 1, count, _file)) > 0) {
                    length += size;
                    count -= size;
                    buf = static_cast<char*>(buf) + size;
                }
                return static_cast<ssize_t>(length);
            }
            return -1;
        }
#endif
        Bytes
        Receive() const
        {
            Bytes bytes;
            ssize_t size;
            size_t length = 0;
            bytes.resize(4096);
            while ((size = Receive(const_cast<byte*>(bytes.data()) + length, 4096)) > 0) {
                length += static_cast<size_t>(size);
                bytes.resize(length + 4096);
            }
            bytes.resize(length);
            return bytes;
        }
    };

    class Sender : public FileHandler
    {
    public:
        using FileHandler::FileHandler;
#ifdef _WIN32
        ssize_t
        Send(const void* buf, size_t count) const
        {
            if (IsFileId()) {
                DWORD size;
                WriteFile(_id, buf, count, &size, NULL);
                return size;
            } else if (IsFILE()) {
                return fwrite(buf, 1, count, _file);
            } else if (IsFileDescriptor()) {
                return _write(_fd, buf, count);
            }
            return -1;
        }
#else
        ssize_t
        Send(const void* buf, size_t count) const
        {
            if (IsFileId()) {
                return write(_id, buf, count);
            } else if (IsFILE()) {
                return static_cast<ssize_t>(fwrite(buf, 1, count, _file));
            }
            return -1;
        }
#endif
        ssize_t
        Send(const Bytes& v) const
        { return Send(v.data(), v.size()); }

        ssize_t
        Send(const std::string& s) const
        { return Send(s.c_str(), s.size()); }
    };

    std::pair<Receiver*, Sender*>
    Pipe()
    {
#ifdef _WIN32
        HANDLE fh[2];
        // make the handles inheritable
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof sa;
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        //
        CreatePipe(fh, fh + 1, &sa, 0) or _throw(OSError("CreatePipe"));
        return {new Receiver(fh[0], true), new Sender(fh[1], true)};
#else
        int fd[2];
#   ifdef _GNU_SOURCE
        pipe2(fd, O_CLOEXEC) == 0 or _throw(OSError("pipe2(2)"));
        return {new Receiver(fd[0], true), new Sender(fd[1], true)};
#   else
        pipe(fd) == 0 or _throw(OSError("pipe(2)"));
        auto r = new Receiver(fd[0], true);
        auto s = new Sender(fd[1], true);
        r->CloseOnExec(true);
        s->CloseOnExec(true);
        return {r, s};
#   endif
#endif
    }
};

class _PIPE
{};
class _STDOUT
{};
class _DEVNUL
{};
class _INFINITE_TIME
{};
static const _PIPE PIPE;
static const _STDOUT STDOUT;
static const _DEVNUL DEVNUL;
static const _INFINITE_TIME INFINITE_TIME;

struct SubprocessError : std::exception
{
    Bytes output;
    Bytes error;
    retcode returncode;
    const std::vector<std::string>& args;

    SubprocessError
    (   const std::vector<std::string>& args_
    ,   retcode returncode_
    ,   const Bytes& output_ = {}
    ,   const Bytes& error_ = {}
    )
    :   output(std::move(output_))
    ,   error(std::move(error_))
    ,   returncode(returncode_)
    ,   args(args_)
    {}

    const char*
    what() const noexcept
    {
        std::string a;
        for (const auto& i : args) {
            if (i.empty()) {
                a += " \"\"";
            } else {
                a += " " + i;
            }
        }
        a = "SubprocessError\n"
            "Arguments:" + a + "\n"
            "Return code: " + std::to_string(returncode) + "\n"
            "Output: " + (output.size() <= 10 ? output.string() : output.string().substr(0, 10) + "[...]") + "\n"
            "Error: " + (error.size() <= 10 ? error.string() : error.string().substr(0, 10) + "[...]");
        char* m = new char[a.size() + 1];
        memcpy(m, a.data(), a.size() + 1);
        return m;
    }
};

struct CalledProcessError : public SubprocessError
{
    using SubprocessError::SubprocessError;
};

struct TimeoutExpired : public SubprocessError
{
    using SubprocessError::SubprocessError;
};

struct ProcessStillActive : public SubprocessError
{
    using SubprocessError::SubprocessError;
};

struct WaitLockMissed : public SubprocessError
{
    using SubprocessError::SubprocessError;
};

class Stream
{
protected:
    inline static FileHandler _devnull;

    static FileHandler&
    _GetDevNull()
    {
        if (_devnull.IsValid()) {
            return _devnull;
        }
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof sa;
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        _devnull.Id(CreateFileA("nul", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa, OPEN_EXISTING, 0, NULL));
        _devnull.IsValid() or _throw(OSError("CreateFileA"));
#else
        _devnull.Id(open("/dev/null", O_RDWR | O_CLOEXEC));
        _devnull.IsValid() or _throw(OSError("open(2)"));
#endif
        _devnull.IsSelfClosing(true);
        return _devnull;
    }
};

class InputStream : Stream
{
private:
    std::unique_ptr<Pipe::Receiver> _receiver;
    std::unique_ptr<Pipe::Sender> _sender;

public:
    const std::unique_ptr<Pipe::Receiver>&
    Receiver() const
    { return _receiver; }

    const std::unique_ptr<Pipe::Sender>&
    Sender() const
    { return _sender; }

    InputStream()
    {}

    InputStream(InputStream&) = delete;

    InputStream(InputStream&& o)
    {
        _receiver = std::move(o._receiver);
        _sender = std::move(o._sender);
    }
    InputStream(file_id id, bool self_closing = false)
    :   _receiver(new Pipe::Receiver(id, self_closing))
    {}

    InputStream(FILE* file, bool self_closing = false)
    :   _receiver(new Pipe::Receiver(file, self_closing))
    {}
#ifdef _WIN32
    InputStream(int fd, bool self_closing = false)
    :   _receiver(new Pipe::Receiver(fd, self_closing))
    {}
#endif
    InputStream(decltype (nullptr))
    {}

    InputStream(const _PIPE&)
    {
        auto pipe = Pipe::Pipe();
        _receiver.reset(pipe.first);
        _sender.reset(pipe.second);
#ifdef _WIN32
        // disable inheritance of _sender by child process
        _sender->MakeInheritable(false);
#endif
    }
    InputStream(const _DEVNUL&)
    :   _receiver(new Pipe::Receiver(_GetDevNull().Id()))
    {}

    void
    DestroyReceiver()
    { _receiver.reset(); }

    InputStream&
    operator=(InputStream&) = delete;

    InputStream&
    operator=(InputStream&& o)
    {
        _receiver = std::move(o._receiver);
        _sender = std::move(o._sender);
        return *this;
    }
};

class OutputStream : Stream
{
private:
    std::unique_ptr<Pipe::Receiver> _receiver;
    std::unique_ptr<Pipe::Sender> _sender;

public:
    const std::unique_ptr<Pipe::Receiver>&
    Receiver() const
    { return _receiver; }

    const std::unique_ptr<Pipe::Sender>&
    Sender() const
    { return _sender; }

    OutputStream()
    {}

    OutputStream(OutputStream&) = delete;

    OutputStream(OutputStream&& o)
    {
        _receiver = std::move(o._receiver);
        _sender = std::move(o._sender);
        o._receiver = nullptr;
        o._sender = nullptr;
    }

    OutputStream(file_id id, bool self_closing = false)
    :   _sender(new Pipe::Sender(id, self_closing))
    {}

    OutputStream(FILE* file, bool self_closing = false)
    :   _sender(new Pipe::Sender(file, self_closing))
    {}
#ifdef _WIN32
    OutputStream(int fd, bool self_closing = false)
    :   _sender(new Pipe::Sender(fd, self_closing))
    {}
#endif
    OutputStream(decltype (nullptr))
    {}

    OutputStream(const _PIPE&)
    {
        auto pipe = Pipe::Pipe();
        _receiver.reset(pipe.first);
        _sender.reset(pipe.second);
#ifdef _WIN32
        // disable inheritance of _receiver by child process
        _receiver->MakeInheritable(false);
#endif
    }
    OutputStream(const _DEVNUL&)
    :   _sender(new Pipe::Sender(_GetDevNull().Id()))
    {}

    void
    DestroySender()
    { _sender.reset(); }

    OutputStream&
    operator=(OutputStream&) = delete;

    OutputStream&
    operator=(OutputStream&& o)
    {
        _receiver = std::move(o._receiver);
        _sender = std::move(o._sender);
        return *this;
    }
};

class ErrorStream : Stream
{
private:
    std::unique_ptr<Pipe::Receiver> _receiver;
    std::unique_ptr<Pipe::Sender> _sender;
    bool _stdout = false;

public:
    const std::unique_ptr<Pipe::Receiver>&
    Receiver() const
    { return _receiver; }

    const std::unique_ptr<Pipe::Sender>&
    Sender() const
    { return _sender; }

    bool
    IsStdOut() const
    { return _stdout; }

    ErrorStream()
    {}

    ErrorStream(ErrorStream&) = delete;

    ErrorStream(ErrorStream&& o)
    {
        _receiver = std::move(o._receiver);
        _sender = std::move(o._sender);
        _stdout = o._stdout;
        o._stdout = false;
    }

    ErrorStream(file_id id, bool self_closing = false)
    :   _sender(new Pipe::Sender(id, self_closing))
    {}

    ErrorStream(FILE* file, bool self_closing = false)
    :   _sender(new Pipe::Sender(file, self_closing))
    {}
#ifdef _WIN32
    ErrorStream(int fd, bool self_closing = false)
    :   _sender(new Pipe::Sender(fd, self_closing))
    {}
#endif
    ErrorStream(decltype (nullptr))
    {}

    ErrorStream(const _PIPE&)
    {
        auto pipe = Pipe::Pipe();
        _receiver.reset(pipe.first);
        _sender.reset(pipe.second);
#ifdef _WIN32
        // disable inheritance of _receiver by child process
        _receiver->MakeInheritable(false);
#endif
    }

    ErrorStream(const _STDOUT&)
    :   _stdout(true)
    {}

    ErrorStream(const _DEVNUL&)
    :   _sender(new Pipe::Sender(_GetDevNull().Id()))
    {}

#ifdef _WIN32
    void
    OutputStream(const OutputStream& stream)
    { _sender.reset(new Pipe::Sender(stream.Sender() != nullptr and stream.Sender()->IsValid() ? stream.Sender()->Id() : GetStdHandle(STD_OUTPUT_HANDLE))); }
#else
    void
    OutputStream(const OutputStream& stream)
    { _sender.reset(new Pipe::Sender(stream.Sender() != nullptr and stream.Sender()->IsValid() ? stream.Sender()->Id() : STDOUT_FILENO)); }
#endif

    void
    DestroySender()
    { _sender.reset(); }

    ErrorStream&
    operator=(ErrorStream&) = delete;

    ErrorStream&
    operator=(ErrorStream&& o)
    {
        _receiver = std::move(o._receiver);
        _sender = std::move(o._sender);
        _stdout = o._stdout;
        return *this;
    }
};

struct Popen;
class Popen_impl
{
protected:
    std::vector<std::string> _args;
    bool _args_is_seq;
    InputStream  _std_in;
    OutputStream _std_out;
    ErrorStream  _std_err;
#ifndef _WIN32
    bool _restore_signals;
#endif
    bool _close_fds;

#ifdef _WIN32
    process_id _ph;
    DWORD _pid;
#else
    process_id _pid;
    std::shared_ptr<std::mutex> _waitpid_lock;
#endif
    retcode _returncode;
    enum {
        sInitial,
        sProcessStarted,
        sEnd
    } _state = sInitial;

public:
    Popen_impl() = default;

    Popen_impl(Popen_impl&) = delete;

    Popen_impl&
    operator=(Popen_impl&) = delete;

    Popen_impl(Popen_impl&& o)
    { *this = std::move(o); }

    Popen_impl&
    operator=(Popen_impl&& o)
    {
        if (_state == sInitial and o._state == sInitial) {
            return *this;
        }
        _state = o._state;
        // avoid closing the handle in the destructor
        o._state = sInitial;
        _pid = o._pid;
#ifdef _WIN32
        _ph = o._ph;
#else
        _waitpid_lock = std::move(o._waitpid_lock);
#endif
        _returncode = o._returncode;
        return *this;
    }

    ~Popen_impl()
    {
        if (_state != sInitial) {
            // Wait for the process to terminate, to avoid zombies.
#ifdef _WIN32
            WaitForSingleObject(_ph, INFINITE);
            CloseHandle(_ph);
#else
            waitpid(_pid, nullptr, 0);
#endif
        }
    }

    void
    Start(Popen& p) noexcept(false);

#ifdef _WIN32
    retcode
    Wait(Popen& p, const _INFINITE_TIME& = INFINITE_TIME) noexcept(false)
    { return Wait(p, INFINITE); }

    retcode
    Wait(Popen& p, duration timeout_ms) noexcept(false)
    {
        if (_state == sEnd) {
            return _returncode;
        }
        Start(p);
        WaitForSingleObject(_ph, timeout_ms) != WAIT_TIMEOUT or _throw(TimeoutExpired(_args, timeout_ms));
        _state = sEnd;
        GetExitCodeProcess(_ph, &_returncode);
        return _returncode;
    }
#else
    retcode
    Wait(Popen& p, const _INFINITE_TIME& = INFINITE_TIME) noexcept(false)
    {
        if (_state == sEnd) {
            return _returncode;
        }
        Start(p);
        while (_state != sEnd) {
            // make sure the mutex is unlocked when going out of scope
            const std::lock_guard<std::mutex> lock (*_waitpid_lock);
            if (_state == sEnd) {
                // Another thread waited.
                return _returncode;
            }
            int status;
            auto ret = _Wait(status, 0);
            // Check the pid and loop as waitpid has been known to
            // return 0 even without WNOHANG in odd situations.
            if (ret == _pid) {
                _HandleExitStatus(status);
                _state = sEnd;
                return _returncode;
            }
        }
        return _returncode;
    }

    retcode
    Wait(Popen& p, duration timeout_ms) noexcept(false)
    {
        if (_state == sEnd) {
            return _returncode;
        }
        Start(p);
        auto end_time = clock::now() + std::chrono::milliseconds(timeout_ms);
        auto delay = std::chrono::microseconds(500);
        auto bound = std::chrono::microseconds(50000);
        // make sure the mutex is unlocked when going out of scope
        std::unique_lock<std::mutex> lock (*_waitpid_lock, std::defer_lock);
        while (true) {
            if (lock.try_lock()) {
                if (_state == sEnd) {
                    // Another thread waited.
                    return _returncode;
                }
                int status;
                if (_Wait(status, WNOHANG) == _pid) {
                    _HandleExitStatus(status);
                    _state = sEnd;
                    return _returncode;
                }
                lock.unlock();
            }
            auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(end_time - clock::now());
            if (remaining.count() <= 0) {
                _throw(TimeoutExpired(_args, timeout_ms));
            }
            delay = std::min(std::min(2 * delay, remaining), bound);
            std::this_thread::sleep_for(delay);
        }
        return _returncode;
    }
#endif

    Return
    Communicate
    (                    Popen& p
    ,              const Bytes& input = {}
    ,   const _INFINITE_TIME& = INFINITE_TIME
    ) noexcept(false)
    {
        if (_state == sEnd) {
            return {};
        }
        Start(p);
        Return ret;
        if ((_std_in.Sender() == nullptr and (_std_out.Receiver() == nullptr or _std_err.Receiver() == nullptr))
            or (_std_out.Receiver() == nullptr and _std_err.Receiver() == nullptr)) {
            if (_std_in.Sender() != nullptr) {
                if (not input.empty()) {
                    _std_in.Sender()->Send(input);
                }
                _std_in.Sender()->Close();
            } else if (_std_out.Receiver() != nullptr) {
                ret.output = _std_out.Receiver()->Receive();
                _std_out.Receiver()->Close();
            } else if (_std_err.Receiver() != nullptr) {
                ret.error = _std_err.Receiver()->Receive();
                _std_err.Receiver()->Close();
            }
            Wait(p);
            return ret;
        } else {
#ifndef _WIN32
            if (_std_in.Sender() != nullptr) {
                fsync(_std_in.Sender()->Id());
            }
#endif
            // send input data
            if (_std_in.Sender() != nullptr and not input.empty()) {
                _std_in.Sender()->Send(input);
                _std_in.Sender()->Close();
            }
            std::future<void> _output_future;
            std::future<void> _error_future;
            // start receiver threads
            if (_std_out.Receiver() != nullptr) {
                auto& output = ret.output;
                auto receiver = _std_out.Receiver().get();
                _output_future = std::async(std::launch::async, [&output, receiver] { output = receiver->Receive(); });
            }
            if (_std_err.Receiver() != nullptr) {
                auto& error = ret.error;
                auto receiver = _std_err.Receiver().get();
                _error_future = std::async(std::launch::async, [&error, receiver] { error = receiver->Receive(); });
            }
            // wait for threads
            if (_std_out.Receiver() != nullptr) {
                _output_future.wait();
            }
            if (_std_err.Receiver() != nullptr) {
                _error_future.wait();
            }
            Wait(p);
            return ret;
        }
    }

    Return
    Communicate
    (         Popen& p
    ,   const Bytes& input
    ,       duration timeout_ms
    ) noexcept(false)
    {
        if (_state == sEnd) {
            return {};
        }
        Start(p);
        Return ret;
        auto end_time = clock::now() + std::chrono::milliseconds(timeout_ms);
#ifndef _WIN32
        if (_std_in.Sender() != nullptr) {
            fsync(_std_in.Sender()->Id());
        }
#endif
        // send input data
        if (_std_in.Sender() != nullptr and not input.empty()) {
            _std_in.Sender()->Send(input);
            _std_in.Sender()->Close();
        }
        std::future<void> _output_future;
        std::future<void> _error_future;
        // start receiver threads
        if (_std_out.Receiver() != nullptr) {
            auto& output = ret.output;
            auto receiver = _std_out.Receiver().get();
            _output_future = std::async(std::launch::async, [&output, receiver] { output = receiver->Receive(); });
        }
        if (_std_err.Receiver() != nullptr) {
            auto& error = ret.error;
            auto receiver = _std_err.Receiver().get();
            _error_future = std::async(std::launch::async, [&error, receiver] { error = receiver->Receive(); });
        }
        // wait for threads
        if (_std_out.Receiver() != nullptr and _output_future.wait_until(end_time) != std::future_status::ready) {
            _throw(TimeoutExpired(_args, timeout_ms));
        }
        if (_std_err.Receiver() != nullptr and _error_future.wait_until(end_time) != std::future_status::ready) {
            _throw(TimeoutExpired(_args, timeout_ms));
        }
        auto remaining = end_time - clock::now();
        if (remaining.count() <= 0) {
            _throw(TimeoutExpired(_args, timeout_ms));
        }
        Wait(p, std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count());
        return ret;
    }

#ifdef _WIN32
    retcode
    Poll(Popen& p) noexcept(false)
    {
        if (_state == sEnd) {
            return _returncode;
        }
        Start(p);
        int ret = WaitForSingleObject(_ph, 0);
        ret != STILL_ACTIVE or _throw(ProcessStillActive(_args, 0));
        ret == WAIT_OBJECT_0 or _throw(OSError("WaitForSingleObject"));
        _state = sEnd;
        GetExitCodeProcess(_ph, &_returncode);
        return _returncode;
    }
#else
    retcode
    Poll(Popen& p) noexcept(false)
    {
        if (_state == sEnd) {
            return _returncode;
        }
        Start(p);
        // make sure the mutex is unlocked when going out of scope
        std::unique_lock<std::mutex> lock (*_waitpid_lock, std::defer_lock);
        lock.try_lock() or _throw(WaitLockMissed(_args, 0));
        if (_state == sEnd) {
            // Another thread waited.
            return _returncode;
        }
        int status;
        auto ret = _Wait(status, WNOHANG);
        if (ret == 0) {
            _throw(ProcessStillActive(_args, 0));
        } else if (ret == _pid) {
            _HandleExitStatus(status);
        } else {
            _returncode = status;
        }
        _state = sEnd;
        return _returncode;
    }
#endif

#ifdef _WIN32
    int
    SendSignal(int sig)
    {
        if (_state == sEnd) {
            return 0;
        }
        if (sig == SIGTERM) {
            return Terminate();
        } else if (sig == CTRL_C_EVENT) {
            return GenerateConsoleCtrlEvent(CTRL_C_EVENT, _pid) != 0;
        } else if (sig == CTRL_BREAK_EVENT) {
            return GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, _pid) != 0;
        }
        return 1;
    }
#else
    int
    SendSignal(int sig)
    {
        if (_state == sEnd) {
            return 0;
        }
        return kill(_pid, sig);
    }
#endif

#ifdef _WIN32
    int
    Terminate()
    {
        if (_state == sEnd) {
            return 0;
        }
        int ret = TerminateProcess(_ph, 1);
        if (ret != 0) {
            return 0;
        }
        _state = sEnd;
        GetExitCodeProcess(_ph, &_returncode);
        return _returncode == STILL_ACTIVE;
    }
#else
    int
    Terminate()
    {
        if (_state == sEnd) {
            return 0;
        }
        return kill(_pid, SIGTERM);
    }
#endif

#ifdef _WIN32
    int
    Kill()
    { return Terminate(); }
#else
    int
    Kill()
    {
        if (_state == sEnd) {
            return 0;
        }
        return kill(_pid, SIGKILL);
    }
#endif

#ifdef _WIN32
    DWORD
    Pid() const
    { return _pid; }
#else
    pid_t
    Pid() const
    { return _pid; }
#endif
    retcode
    ReturnCode() const
    { return _returncode; }

    std::vector<std::string>
    Arguments() const
    { return _args; }

protected:
#ifdef _WIN32
    std::unique_ptr<STARTUPINFO>
    _GetStartupInfo()
    {
        bool v0 = _std_in .Receiver() != nullptr and _std_in .Receiver()->IsValid();
        bool v1 = _std_out.Sender  () != nullptr and _std_out.Sender  ()->IsValid();
        bool v2 = _std_err.Sender  () != nullptr and _std_err.Sender  ()->IsValid();
        auto si = new STARTUPINFO();
        si->cb = sizeof (STARTUPINFO);
        if (v0 or v1 or v2) {
            si->dwFlags = STARTF_USESTDHANDLES;
            si->hStdInput  = v0 ? _std_in .Receiver()->Id() : GetStdHandle(STD_INPUT_HANDLE);
            si->hStdOutput = v1 ? _std_out.Sender  ()->Id() : GetStdHandle(STD_OUTPUT_HANDLE);
            si->hStdError  = v2 ? _std_err.Sender  ()->Id() : GetStdHandle(STD_ERROR_HANDLE);
            _close_fds = false;
        }
        return std::unique_ptr<STARTUPINFO>(si);
    }

    std::unique_ptr<std::string, void (*)(std::string*)>
    _GetCommand()
    {
        if (not _args_is_seq) {
            return {&_args.front(), _noopDeleter};
        }
        auto s = new std::string(shellEscaped(_args.front()));
        auto i = _args.cbegin() + 1, e = _args.cend();
        for (; i != e; ++i) {
            s->append(" " + shellEscaped(*i));
        }
        return {s, _deleteString};
    }

    std::unique_ptr<char[]>
    _GetEnvironment(Popen& p) const;
#else
    std::unique_ptr<posix_spawn_file_actions_t, decltype (&posix_spawn_file_actions_destroy)>
    _GetFileActions()
    {
        bool v0 = _std_in .Sender  () != nullptr and _std_in .Sender  ()->IsValid();
        bool v1 = _std_out.Receiver() != nullptr and _std_out.Receiver()->IsValid();
        bool v2 = _std_err.Receiver() != nullptr and _std_err.Receiver()->IsValid();
        bool v3 = _std_in .Receiver() != nullptr and _std_in .Receiver()->IsValid();
        bool v4 = _std_out.Sender  () != nullptr and _std_out.Sender  ()->IsValid();
        bool v5 = _std_err.Sender  () != nullptr and _std_err.Sender  ()->IsValid();
        if (not (v0 or v1 or v2 or v3 or v4 or v5 or _close_fds)) {
            return {nullptr, nullptr};
        }
        if (v3 and _std_in .Receiver()->Id() > STDERR_FILENO) {
            _std_in .Receiver()->CloseOnExec(true);
        }
        if (v4 and _std_out.Sender  ()->Id() > STDERR_FILENO) {
            _std_out.Sender  ()->CloseOnExec(true);
        }
        if (v5 and _std_err.Sender  ()->Id() > STDERR_FILENO) {
            _std_err.Sender  ()->CloseOnExec(true);
        }
        int max_fd;
        if (_close_fds) {
            max_fd = sysconf(_SC_OPEN_MAX);
            max_fd != -1 or _throw(OSError("sysconf(3)"));
        }
        auto actions = new posix_spawn_file_actions_t;
        posix_spawn_file_actions_init(actions);
        v0 and posix_spawn_file_actions_addclose(actions, _std_in .Sender  ()->Id());
        v1 and posix_spawn_file_actions_addclose(actions, _std_out.Receiver()->Id());
        v2 and posix_spawn_file_actions_addclose(actions, _std_err.Receiver()->Id());
        v3 and posix_spawn_file_actions_adddup2 (actions, _std_in .Receiver()->Id(), STDIN_FILENO );
        v4 and posix_spawn_file_actions_adddup2 (actions, _std_out.Sender  ()->Id(), STDOUT_FILENO);
        v5 and posix_spawn_file_actions_adddup2 (actions, _std_err.Sender  ()->Id(), STDERR_FILENO);
        if (_close_fds) {
            for (int i = STDERR_FILENO + 1; i < max_fd; ++i) {
                posix_spawn_file_actions_addclose(actions, i);
            }
        }
        return {actions, posix_spawn_file_actions_destroy};
    }

    std::unique_ptr<posix_spawnattr_t, decltype (&posix_spawnattr_destroy)>
    _GetAttributes()
    {
        if (not _restore_signals) {
            return {nullptr, nullptr};
        }
        auto attr = new posix_spawnattr_t;
        posix_spawnattr_init(attr);
        posix_spawnattr_setflags(attr, POSIX_SPAWN_SETSIGDEF);
        sigset_t set;
        sigemptyset(&set);
#   ifdef SIGPIPE
        sigaddset(&set, SIGPIPE);
#   endif
#   ifdef SIGXFZ
        sigaddset(&set, SIGXFZ );
#   endif
#   ifdef SIGXFSZ
        sigaddset(&set, SIGXFSZ);
#   endif
        posix_spawnattr_setsigdefault(attr, &set);
        return {attr, posix_spawnattr_destroy};
    }

    std::unique_ptr<char*[], void (*)(char**)>
    _GetArguments()
    {
        // the size check is already done in Start()
        if (not _args_is_seq) {
            return shellSplitted(_args.front().c_str());
        }
        auto argv = new char*[_args.size() + 1];
        auto p = argv;
        for (const auto& arg : _args) {
            *p = const_cast<char*>(arg.c_str());
            ++p;
        }
        *p = nullptr;
        return {argv, _deleteOneArray};
    }

    std::unique_ptr<char*[]>
    _GetEnvironment(Popen& p);

    void
    _Exec(Popen& p);

    pid_t
    _Wait(int& status, int options) const noexcept(false)
    {
        auto ret = waitpid(_pid, &status, options);
        if (ret != -1) {
            return ret;
        }
        errno == ECHILD or _throw(OSError("waitpid(3)"));
        // This happens if SIGCLD is set to be ignored or waiting for child processes
        // has otherwise been disabled for our process.  This child is dead, we can't
        // get the status.
        status = 0;
        return _pid;
    }

    void
    _HandleExitStatus(int status)
    {
        if (WIFSTOPPED(status)) {
            _returncode = -WSTOPSIG(status);
        } else if (WIFEXITED(status)) {
            _returncode = WEXITSTATUS(status);
        } else {
            _returncode = WTERMSIG(status);
        }
    }
#endif
};

/**
 * \example subprocess.h
 * \code
 * sp::Popen{{"cmd.exe /c echo Hello world!"}}.Wait();
 *
 * sp::Popen{{"cmd.exe", "/c", "echo", "Hello world!"}, true}.Wait();
 *
 * sp::Popen().Command("cmd.exe /c echo Hello world!").Wait();
 *
 * sp::Popen().Arguments({"cmd.exe", "/c", "echo", "Hello world!"}).Wait();
 *
 * auto p1 = sp::Popen().Command("cmd.exe /c echo Hello world!").Start()();
 * // ...
 * p1.Wait();
 *
 * FILE* f = fopen("tmp.txt", "r");
 * sp::Popen().Arguments("a.exe").StdIn(f).Wait();
 * fclose(f);
 * // alternatively
 * sp::Popen().Arguments("a.exe").StdIn({fopen("tmp.txt", "r"), true}).Wait();
 *
 * sp::Popen p2;
 * p2.Command("ping -n 10 127.0.0.1").StdErr(sp::DEVNUL);
 * try {
 *     p2.Wait(100);
 * } catch (const sp::TimeoutExpired&) {
 *     p2.Terminate();
 * }
 * \endcode
 *
 * \todo add some of the remaining parameters; see python doc
 */
struct Popen
{
    std::vector<std::string> args;
    bool args_is_seq;
    InputStream  std_in;
    OutputStream std_out;
    ErrorStream  std_err;
    std::string cwd;
    std::vector<std::string> env;
#ifdef _WIN32
    unsigned creation_flags = 0;
#else
    bool restore_signals = true;
#endif
    bool close_fds = true;

    std::unique_ptr<Popen_impl> _impl;

    Popen() = default;

    Popen(Popen&) = delete;

    Popen(Popen&& o) = default;

    Popen&
    operator=(Popen&) = delete;

    Popen&
    operator=(Popen&& o) = default;

    Popen
    Self()
    { return std::move(*this); }

    Popen
    operator()()
    { return std::move(*this); }

    Popen&
    Arguments(const std::string& cmd)
    {
        args.clear();
        args.push_back(cmd);
        args_is_seq = false;
        return *this;
    }

    Popen&
    Arguments(const std::vector<std::string>& args)
    {
        this->args = args;
        args_is_seq = true;
        return *this;
    }

    Popen&
    Arguments(std::initializer_list<const char*> args)
    {
        this->args.clear();
        this->args.assign(args.begin(), args.end());
        args_is_seq = true;
        return *this;
    }

    Popen&
    Command(const std::string& cmd)
    { return Arguments(cmd); }

    Popen&
    Command(const std::vector<std::string>& args)
    { return Arguments(args); }

    Popen&
    Command(std::initializer_list<const char*> args)
    { return Arguments(args); }

    Popen&
    StdIn(InputStream&& std_in)
    {
        this->std_in = std::move(std_in);
        return *this;
    }

    Popen&
    StdOut(OutputStream&& std_out)
    {
        this->std_out = std::move(std_out);
        return *this;
    }

    Popen&
    StdErr(ErrorStream&& std_err)
    {
        this->std_err = std::move(std_err);
        return *this;
    }

    Popen&
    Directory(const std::string& cwd)
    {
        this->cwd = cwd;
        return *this;
    }

    /**
     * @param env An array of key=values strings.
     */
    Popen&
    Environment(const std::vector<std::string>& env)
    {
        this->env = env;
        return *this;
    }
#ifdef _WIN32
    Popen&
    CreationFlags(unsigned flags)
    {
        this->creation_flags = flags;
        return *this;
    }
#else
    Popen&
    RestoreSignals(bool restore_signals_)
    {
        restore_signals = restore_signals_;
        return *this;
    }
#endif
    Popen&
    CloseFileDescriptors(bool close_fds)
    {
        this->close_fds = close_fds;
        return *this;
    }

    Popen&
    InheritHandles(bool inherit)
    { return CloseFileDescriptors(not inherit); }

    Popen&
    Start() noexcept(false)
    {
        Impl()->Start(*this);
        return *this;
    }

    /**
     * @brief Wait for child process to terminate. Set and return the returncode.
     * @return retcode.
     */
    retcode
    Wait(const _INFINITE_TIME& = INFINITE_TIME) noexcept(false)
    { return Impl()->Wait(*this); }

    /**
     * @brief Wait for child process to terminate in timeout_ms milliseconds. Set and return the returncode.
     * @param timeout_ms Timeout in milliseconds.
     * @return retcode.
     */
    retcode
    Wait(duration timeout_ms) noexcept(false)
    { return Impl()->Wait(*this, timeout_ms); }

    Return
    Communicate(const Bytes& input = {}, const _INFINITE_TIME& = INFINITE_TIME) noexcept(false)
    { return Impl()->Communicate(*this, input); }

    Return
    Communicate(const Bytes& input, duration timeout_ms) noexcept(false)
    { return Impl()->Communicate(*this, input, timeout_ms); }

    Return
    Communicate(duration timeout_ms, const Bytes& input = {}) noexcept(false)
    { return Impl()->Communicate(*this, input, timeout_ms); }

    Return
    Communicate(const _INFINITE_TIME&, const Bytes& input = {}) noexcept(false)
    { return Impl()->Communicate(*this, input); }

    retcode
    Poll() noexcept(false)
    { return Impl()->Poll(*this); }

    int
    SendSignal(int sig)
    { return Impl()->SendSignal(sig); }

    int
    Terminate()
    { return Impl()->Terminate(); }

    int
    Kill()
    { return Impl()->Kill(); }

#ifdef _WIN32
    DWORD
#else
    pid_t
#endif
    Pid() const
    { return _impl->Pid(); }

    retcode
    ReturnCode() const
    { return _impl->ReturnCode(); }

    std::vector<std::string>
    Arguments() const
    { return _impl->Arguments(); }

    Popen_impl*
    Impl()
    {
        if (not _impl) {
            _impl.reset(new Popen_impl);
        }
        return _impl.get();
    }
};

#ifdef _WIN32
void
Popen_impl::
Start(Popen& p) noexcept(false)
{
    if (_state != sInitial) {
        return;
    }
    (not p.args.empty() and not p.args.front().empty()) or _throw(std::invalid_argument("Invalid or no arguments provided"));
    _args = p.args;
    _args_is_seq = p.args_is_seq;
    _close_fds = p.close_fds;
    _std_in = std::move(p.std_in);
    _std_out = std::move(p.std_out);
    _std_err = std::move(p.std_err);
    if (_std_err.IsStdOut()) {
        _std_err.OutputStream(_std_out);
    }
    // setup
    auto si = _GetStartupInfo();
    auto pi = std::unique_ptr<PROCESS_INFORMATION>(new PROCESS_INFORMATION());
    auto cmd = _GetCommand();
    auto env = _GetEnvironment(p);
    auto cwd = p.cwd.empty() ? nullptr : p.cwd.c_str();
    // run
    CreateProcessA(nullptr, cmd->data(), nullptr, nullptr, not _close_fds, p.creation_flags, env.get(), cwd, si.get(), pi.get())
    or _throw(OSError("CreateProcessA"));
    _ph = pi->hProcess;
    _pid = pi->dwProcessId;
    _state = sProcessStarted;
    // cleanup
    CloseHandle(pi->hThread);
    _std_in.DestroyReceiver();
    _std_out.DestroySender();
    _std_err.DestroySender();
}

std::unique_ptr<char[]>
Popen_impl::
_GetEnvironment(Popen& p) const
{
    size_t size = 1u;
    for (const auto& s : p.env) {
        size += s.size() + 1;
    }
    if (size == 1) {
        return nullptr;
    }
    auto env = new char[size];
    auto ptr = env;
    for (const auto& s : p.env) {
        size = s.size() + 1;
        memcpy(ptr, s.data(), size);
        ptr += size;
    }
    *ptr = '\0';
    return std::unique_ptr<char[]>(env);
}
#else
void
Popen_impl::
Start(Popen& p) noexcept(false)
{
    if (_state != sInitial) {
        return;
    }
    (not p.args.empty() and not p.args.front().empty()) or _throw(std::invalid_argument("Invalid or no arguments provided"));
    _args = p.args;
    _args_is_seq = p.args_is_seq;
    _close_fds = p.close_fds;
    _restore_signals = p.restore_signals;
    _std_in = std::move(p.std_in);
    _std_out = std::move(p.std_out);
    _std_err = std::move(p.std_err);
    if (_std_err.IsStdOut()) {
        _std_err.OutputStream(_std_out);
    }
    if (p.cwd.empty()) {
        // setup
        auto file_actions = _GetFileActions();
        auto attrp = _GetAttributes();
        auto argv = _GetArguments();
        auto env = _GetEnvironment(p);
        // run
        posix_spawnp(&_pid, argv[0], file_actions.get(), attrp.get(), argv.get(), env.get()) == 0
        or _throw(OSError("posix_spawnp(3p)"));
    } else {
        _pid = fork();
        _pid >= 0 or _throw(OSError("fork(2)"));
        _Exec(p);
    }
    _state = sProcessStarted;
    // cleanup
    _std_in.DestroyReceiver();
    _std_out.DestroySender();
    _std_err.DestroySender();
    _waitpid_lock.reset(new std::mutex);
}

std::unique_ptr<char*[]>
Popen_impl::
_GetEnvironment(Popen& p)
{
    if (p.env.empty()) {
        // maybe the system's environment should be returned instead of this
        return nullptr;
    }
    auto env = new char*[p.env.size() + 1];
    auto ptr = env;
    for (const auto& e : p.env) {
        *ptr = const_cast<char*>(e.c_str());
        ++ptr;
    }
    *ptr = nullptr;
    return std::unique_ptr<char*[]>(env);
}

void
Popen_impl::
_Exec(Popen& p)
{
    if (_pid != 0) {
        // parents are not allowed here
        return;
    }
    try {
        bool v0 = _std_in .Sender  () != nullptr and _std_in .Sender  ()->IsValid();
        bool v1 = _std_out.Receiver() != nullptr and _std_out.Receiver()->IsValid();
        bool v2 = _std_err.Receiver() != nullptr and _std_err.Receiver()->IsValid();
        bool v3 = _std_in .Receiver() != nullptr and _std_in .Receiver()->IsValid();
        bool v4 = _std_out.Sender  () != nullptr and _std_out.Sender  ()->IsValid();
        bool v5 = _std_err.Sender  () != nullptr and _std_err.Sender  ()->IsValid();
        if (v0 or v1 or v2 or v3 or v4 or v5) {
            if (v3 and _std_in .Receiver()->Id() > STDERR_FILENO) {
                _std_in .Receiver()->CloseOnExec(true);
            }
            if (v4 and _std_out.Sender  ()->Id() > STDERR_FILENO) {
                _std_out.Sender  ()->CloseOnExec(true);
            }
            if (v5 and _std_err.Sender  ()->Id() > STDERR_FILENO) {
                _std_err.Sender  ()->CloseOnExec(true);
            }
            v0 and _std_in .Sender  ()->Close();
            v1 and _std_out.Receiver()->Close();
            v2 and _std_err.Receiver()->Close();
            v3 and (dup2(_std_in .Receiver()->Id(), STDIN_FILENO ) != -1 or _throw(OSError("dup2(2)")));
            v4 and (dup2(_std_out.Sender  ()->Id(), STDOUT_FILENO) != -1 or _throw(OSError("dup2(2)")));
            v5 and (dup2(_std_err.Sender  ()->Id(), STDERR_FILENO) != -1 or _throw(OSError("dup2(2)")));
        }
        if (_close_fds) {
            int max_fd = sysconf(_SC_OPEN_MAX);
            max_fd != -1 or _throw(OSError("sysconf(3)"));
            for (int i = STDERR_FILENO + 1; i < max_fd; ++i) {
                close(i);
            }
        }
        if (_restore_signals) {
#   ifdef SIGPIPE
            sigset(SIGPIPE, SIG_DFL);
#   endif
#   ifdef SIGXFZ
            sigset(SIGXFZ , SIG_DFL);
#   endif
#   ifdef SIGXFSZ
            sigset(SIGXFSZ, SIG_DFL);
#   endif
        }
        if (not p.cwd.empty()) {
            chdir(p.cwd.c_str()) == 0 or _throw(OSError("chdir(2)"));
        }
        auto argv = _GetArguments();
        if (p.env.empty()) {
            execvp(argv[0], argv.get()) or _throw(OSError("execvp(2)"));
        } else {
            auto env1 = _GetEnvironment(p);
            execvpe(argv[0], argv.get(), env1.get()) or _throw(OSError("execvpe(2)"));
        }
    } catch (...) {
        // ignored for now
    }
    _exit(0x7F);
}
#endif

void
_poll(Popen& process, const Return& ret) noexcept(false)
{
    while (true) {
        try {
            process.Poll() == 0 or _throw(nullptr);
            break;
        } catch (const WaitLockMissed&) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(50));
        } catch (...) {
            _throw(CalledProcessError(process.Arguments(), process.ReturnCode(), ret.output, ret.error));
        }
    }
}

Bytes
_check_output(Popen&& process, duration timeout_ms) noexcept(false)
{
    Return ret;
    try {
        ret = process.Communicate(timeout_ms);
    } catch (TimeoutExpired& e) {
        process.Kill();
#ifdef _WIN32
        // Windows accumulates the output in a single blocking
        // Receive() call run on child threads, with the timeout
        // being done in a wait_until() on those threads.
        // Communicate() _after_ Kill() is required to collect
        // that and add it to the exception.
        ret = process.Communicate();
        e.output = ret.output;
        e.error = ret.error;
#else
        // POSIX _communicate() already populated the output so
        // far into the TimeoutExpired exception.
        process.Wait();
#endif
        throw;
    } catch (...) {
        process.Kill();
        throw;
    }
    _poll(process, ret);
    return std::move(ret.output);
}

Bytes
_check_output(Popen&& process, const _INFINITE_TIME& = INFINITE_TIME)
{
    Return ret;
    try {
        ret = process.Communicate();
    } catch (...) {
        process.Kill();
        throw;
    }
    _poll(process, ret);
    return std::move(ret.output);
}

Bytes
check_output
(   const std::string& cmd
,             duration timeout_ms
,        InputStream&& std_in = {}
,        ErrorStream&& std_err = {}
,   const std::string& cwd = {}
)
{
    return _check_output(
        Popen{
            .args = {cmd},
            .std_in = std::move(std_in),
            .std_out = PIPE,
            .std_err = std::move(std_err),
            .cwd = cwd,
        },
        timeout_ms
    );
}

Bytes
check_output
(        const std::string& cmd
,   const _INFINITE_TIME& = INFINITE_TIME
,             InputStream&& std_in = {}
,             ErrorStream&& std_err = {}
,        const std::string& cwd = {}
)
{
    return _check_output(
        Popen{
            .args = {cmd},
            .args_is_seq = false,
            .std_in = std::move(std_in),
            .std_out = PIPE,
            .std_err = std::move(std_err),
            .cwd = cwd,
        }
    );
}

Bytes
check_output
(   const std::vector<std::string>& args
,                          duration timeout_ms
,                     InputStream&& std_in = {}
,                     ErrorStream&& std_err = {}
,                const std::string& cwd = {}
)
{
    return _check_output(
        Popen{
            .args = args,
            .args_is_seq = true,
            .std_in = std::move(std_in),
            .std_out = PIPE,
            .std_err = std::move(std_err),
            .cwd = cwd,
        },
        timeout_ms
    );
}

Bytes
check_output
(   const std::vector<std::string>& args
,           const _INFINITE_TIME& = INFINITE_TIME
,                     InputStream&& std_in = {}
,                     ErrorStream&& std_err = {}
,                const std::string& cwd = {}
)
{
    return _check_output(
        Popen{
            .args = args,
            .args_is_seq = true,
            .std_in = std::move(std_in),
            .std_out = PIPE,
            .std_err = std::move(std_err),
            .cwd = cwd,
        }
    );
}

Bytes
check_output
(   std::initializer_list<const char*> args
,                             duration timeout_ms
,                        InputStream&& std_in = {}
,                        ErrorStream&& std_err = {}
,                   const std::string& cwd = {}
)
{
    return _check_output(
        Popen{
            .args = std::vector<std::string>(args.begin(), args.end()),
            .args_is_seq = true,
            .std_in = std::move(std_in),
            .std_out = PIPE,
            .std_err = std::move(std_err),
            .cwd = cwd,
        },
        timeout_ms
    );
}

Bytes
check_output
(   std::initializer_list<const char*> args
,              const _INFINITE_TIME& = INFINITE_TIME
,                        InputStream&& std_in = {}
,                        ErrorStream&& std_err = {}
,                   const std::string& cwd = {}
)

{
    return _check_output(
        Popen{
            .args = std::vector<std::string>(args.begin(), args.end()),
            .args_is_seq = true,
            .std_in = std::move(std_in),
            .std_out = PIPE,
            .std_err = std::move(std_err),
            .cwd = cwd,
        }
    );
}

int
call
(   const std::string& cmd
,             duration timeout_ms
,        InputStream&& std_in = {}
,       OutputStream&& std_out = {}
,        ErrorStream&& std_err = {}
,   const std::string& cwd = {}
)
{
    return Popen{
        .args = {cmd},
        .args_is_seq = false,
        .std_in = std::move(std_in),
        .std_out = std::move(std_out),
        .std_err = std::move(std_err),
        .cwd = cwd,
    }.Wait(timeout_ms);
}

int
call
(        const std::string& cmd
,   const _INFINITE_TIME& = INFINITE_TIME
,             InputStream&& std_in = {}
,            OutputStream&& std_out = {}
,             ErrorStream&& std_err = {}
,        const std::string& cwd = {}
)
{
    return Popen{
        .args = {cmd},
        .args_is_seq = false,
        .std_in = std::move(std_in),
        .std_out = std::move(std_out),
        .std_err = std::move(std_err),
        .cwd = cwd,
    }.Wait();
}

int
call
(   const std::vector<std::string>& args
,                          duration timeout_ms
,                     InputStream&& std_in = {}
,                    OutputStream&& std_out = {}
,                     ErrorStream&& std_err = {}
,                const std::string& cwd = {}
)
{
    return Popen{
        .args = args,
        .args_is_seq = true,
        .std_in = std::move(std_in),
        .std_out = std::move(std_out),
        .std_err = std::move(std_err),
        .cwd = cwd,
    }.Wait(timeout_ms);
}

int
call
(   const std::vector<std::string>& args
,           const _INFINITE_TIME& = INFINITE_TIME
,                     InputStream&& std_in = {}
,                    OutputStream&& std_out = {}
,                     ErrorStream&& std_err = {}
,                const std::string& cwd = {}
)
{
    return Popen{
        .args = args,
        .args_is_seq = true,
        .std_in = std::move(std_in),
        .std_out = std::move(std_out),
        .std_err = std::move(std_err),
        .cwd = cwd,
    }.Wait();
}

int
call
(   std::initializer_list<const char*> args
,                             duration timeout_ms
,                        InputStream&& std_in = {}
,                       OutputStream&& std_out = {}
,                        ErrorStream&& std_err = {}
,                   const std::string& cwd = {}
)
{
    return Popen{
        .args = std::vector<std::string>(args.begin(), args.end()),
        .args_is_seq = true,
        .std_in = std::move(std_in),
        .std_out = std::move(std_out),
        .std_err = std::move(std_err),
        .cwd = cwd,
    }.Wait(timeout_ms);
}

int
call
(   std::initializer_list<const char*> args
,              const _INFINITE_TIME& = INFINITE_TIME
,                        InputStream&& std_in = {}
,                       OutputStream&& std_out = {}
,                        ErrorStream&& std_err = {}
,                   const std::string& cwd = {}
)
{
    return Popen{
        .args = std::vector<std::string>(args.begin(), args.end()),
        .args_is_seq = true,
        .std_in = std::move(std_in),
        .std_out = std::move(std_out),
        .std_err = std::move(std_err),
        .cwd = cwd,
    }.Wait();
}

}

#endif // SUBPROCESS_H
