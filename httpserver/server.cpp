#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <optional>
#include <cstdint>
#include <cstring>
#include <cerrno>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

namespace cfg {
    inline constexpr short        PORT          = 1235;
    inline constexpr int          BACKLOG       = 16;
    inline constexpr std::size_t  BUFSIZE       = 4096;
    inline constexpr const char*  BASE_ADDR     = "127.0.0.1";
    inline constexpr std::size_t  MAX_REQUEST   = 64 * 1024;   
    inline constexpr int          RECV_TIMEOUT  = 10;         
    inline constexpr const char*  INDEX_PAGE    = "./index.html";
    inline constexpr const char*  ERROR_PAGE    = "404.html";
}


[[noreturn]] static void throw_errno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

class SocketAddress {
    sockaddr_in saddr_{};
public:
    SocketAddress() {
        saddr_.sin_family      = AF_INET;
        saddr_.sin_port        = htons(cfg::PORT);
        saddr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    SocketAddress(std::string_view ip, short port) {
        saddr_.sin_family = AF_INET;
        saddr_.sin_port   = htons(port);                       
        if (::inet_pton(AF_INET, ip.data(), &saddr_.sin_addr) != 1)
            throw std::invalid_argument("bad IP address: " + std::string(ip));
    }

    socklen_t  Len()  const { return sizeof(saddr_); }
    sockaddr*  Addr()       { return reinterpret_cast<sockaddr*>(&saddr_); }
    const sockaddr* Addr() const { return reinterpret_cast<const sockaddr*>(&saddr_); }

    std::string Ip() const {
        char buf[INET_ADDRSTRLEN]{};
        ::inet_ntop(AF_INET, &saddr_.sin_addr, buf, sizeof(buf));
        return buf;
    }
};

class Socket {
protected:
    int sd_{-1};
    explicit Socket(int sd) : sd_(sd) {}
public:
    Socket() {
        sd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sd_ < 0) throw_errno("socket");
    }

    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& o) noexcept : sd_(o.sd_) { o.sd_ = -1; }
    Socket& operator=(Socket&& o) noexcept {
        if (this != &o) {
            Close();
            sd_ = o.sd_;
            o.sd_ = -1;
        }
        return *this;
    }

    ~Socket() { Close(); }

    void Close() noexcept {
        if (sd_ >= 0) { ::close(sd_); sd_ = -1; }
    }

    void Shutdown() noexcept {
        if (sd_ >= 0) ::shutdown(sd_, SHUT_RDWR);
    }

    int Fd() const noexcept { return sd_; }
};

class ConnectedSocket : public Socket {
public:
    ConnectedSocket() : Socket(-1) {}   
    explicit ConnectedSocket(int sd) : Socket(sd) {}

    void SetRecvTimeout(int seconds) {
        timeval tv{seconds, 0};
        ::setsockopt(sd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    void Write(std::string_view data) {
        std::size_t total = 0;
        while (total < data.size()) {
            ssize_t n = ::send(sd_, data.data() + total, data.size() - total, MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw_errno("send");
            }
            total += static_cast<std::size_t>(n);
        }
    }

    void Write(const std::vector<std::uint8_t>& bytes) {
        Write(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }

    std::optional<std::string> ReadRequest() {
        std::string req;
        char buf[cfg::BUFSIZE];
        for (;;) {
            ssize_t n = ::recv(sd_, buf, sizeof(buf), 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                return std::nullopt;                  
            }
            if (n == 0) break;                        
            req.append(buf, static_cast<std::size_t>(n));
            if (req.size() > cfg::MAX_REQUEST)
                throw std::length_error("request too large");
            if (req.find("\r\n\r\n") != std::string::npos) break;
        }
        if (req.empty()) return std::nullopt;
        return req;
    }
};

class ServerSocket : public Socket {
public:
    ServerSocket() {
        int opt = 1;
        ::setsockopt(sd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    void Bind(SocketAddress& addr) {
        if (::bind(sd_, addr.Addr(), addr.Len()) < 0) throw_errno("bind");
    }

    void Listen() {
        if (::listen(sd_, cfg::BACKLOG) < 0) throw_errno("listen");
    }

    ConnectedSocket Accept(SocketAddress& client) {
        socklen_t len = client.Len();
        int cd = ::accept(sd_, client.Addr(), &len);
        if (cd < 0) throw_errno("accept");
        return ConnectedSocket(cd);
    }
};

static std::vector<std::string> split_lines(std::string_view str) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : str) {
        if (c == '\n') {
            if (!cur.empty() && cur.back() == '\r') cur.pop_back();
            out.push_back(std::move(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        if (cur.back() == '\r') cur.pop_back();
        out.push_back(std::move(cur));
    }
    return out;
}

static std::vector<std::uint8_t> read_fd(int fd) {
    std::vector<std::uint8_t> res;
    char buf[cfg::BUFSIZE];
    ssize_t n;
    while ((n = ::read(fd, buf, sizeof(buf))) > 0)
        res.insert(res.end(), buf, buf + n);
    return res;
}

struct RequestLine {
    std::string method;
    std::string target;   
};

static RequestLine parse_request_line(const std::string& line) {
    std::istringstream iss(line);
    RequestLine r;
    iss >> r.method >> r.target;
    return r;
}

static std::string target_to_path(const std::string& target) {
    if (target.empty() || target == "/") return cfg::INDEX_PAGE;
    std::string p = ".";
    if (target.front() != '/') p += '/';
    p += target;
    return p;
}

static bool is_cgi(const std::string& path) {
    return path.find('?') != std::string::npos;
}

static std::string cgi_script(const std::string& path) {
    return path.substr(0, path.find('?'));
}

static std::string cgi_query(const std::string& path) {
    auto q = path.find('?');
    return (q == std::string::npos || q + 1 >= path.size()) ? "" : path.substr(q + 1);
}

static std::string make_header(std::string_view status,
                               std::string_view content_type,
                               std::size_t length) {
    std::ostringstream os;
    os << "HTTP/1.1 " << status << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << length << "\r\n"
       << "Connection: close\r\n\r\n";
    return os.str();
}

template <typename F>
class ScopeGuard {
    F   fn_;
    bool active_{true};
public:
    explicit ScopeGuard(F fn) : fn_(std::move(fn)) {}
    ScopeGuard(const ScopeGuard&)            = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    void Dismiss() noexcept { active_ = false; }
    ~ScopeGuard() { if (active_) fn_(); }
};

static void handle_cgi(const std::string& path, ConnectedSocket& cs, const std::string& request) {
    const std::string script = cgi_script(path);
    const std::string query  = cgi_query(path);

    sigset_t chld_mask, prev_mask;
    sigemptyset(&chld_mask);
    sigaddset(&chld_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chld_mask, &prev_mask);
    ScopeGuard mask_guard([&] { sigprocmask(SIG_SETMASK, &prev_mask, nullptr); });

    char tmpl[] = "cgi_out_XXXXXX";
    int tmp_fd = ::mkstemp(tmpl);
    if (tmp_fd < 0) {
        cs.Write("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n");
        return;
    }
    const std::string out_file = tmpl;
    ScopeGuard file_guard([&] { ::close(tmp_fd); ::unlink(out_file.c_str()); });

    pid_t pid = ::fork();
    if (pid < 0) {
        cs.Write("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n");
        return;
    }

    if (pid == 0) {
        ::dup2(tmp_fd, STDOUT_FILENO);
        ::close(tmp_fd);

        std::vector<std::string> envs = {
            request,
            "SERVER_ADDR=127.0.0.1",
            "SERVER_PORT=1235",
            "SERVER_PROTOCOL=HTTP/1.1",
            "CONTENT_TYPE=text/plain",
            "QUERY_STRING=" + query,
            "SCRIPT_NAME=" + script,
        };
        std::vector<char*> envp;
        envp.reserve(envs.size() + 1);
        for (auto& e : envs) envp.push_back(e.data());
        envp.push_back(nullptr);

        char* argv[] = { const_cast<char*>(script.c_str()), nullptr };
        ::execve(script.c_str(), argv, envp.data());
        _exit(2);   
    }

    int status = 0;
    pid_t w = ::waitpid(pid, &status, 0);        

    if (w < 0) {
        cs.Write("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n");
        return;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        ::lseek(tmp_fd, 0, SEEK_SET);
        std::vector<std::uint8_t> body = read_fd(tmp_fd);

        cs.Write(make_header("200 OK", "text/plain", body.size()));
        cs.Write(body);
        std::cout << "200 OK (cgi), " << body.size() << " bytes\n";
    } else {
        cs.Write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
        std::cout << "404 (cgi exec failed)\n";
    }

    cs.Shutdown();
}

static void handle_static(const std::string& path, ConnectedSocket& cs) {
    int fd = ::open(path.c_str(), O_RDONLY);
    bool ok = (fd >= 0);

    if (!ok) {
        fd = ::open(cfg::ERROR_PAGE, O_RDONLY);
        if (fd < 0) {                                    
            cs.Write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
            std::cout << "404 (no error page)\n";
            return;
        }
    }

    std::vector<std::uint8_t> body = read_fd(fd);
    ::close(fd);

    cs.Write(make_header(ok ? "200 OK" : "404 Not Found", "text/html", body.size()));
    cs.Write(body);
    std::cout << (ok ? "200 OK, " : "404, ") << body.size() << " bytes\n";
    cs.Shutdown();
}

static void process_connection(ConnectedSocket cs, const SocketAddress& client) {
    cs.SetRecvTimeout(cfg::RECV_TIMEOUT);

    try {
        std::optional<std::string> req = cs.ReadRequest();
        if (!req) return;

        auto lines = split_lines(*req);
        if (lines.empty()) return;

        RequestLine rl = parse_request_line(lines[0]);
        std::cout << client.Ip() << " -> " << lines[0] << '\n';

        if (rl.method != "GET") {
            cs.Write("HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\nConnection: close\r\n\r\n");
            return;
        }

        std::string path = target_to_path(rl.target);
        if (is_cgi(path)) handle_cgi(path, cs, *req);
        else              handle_static(path, cs);
    }
    catch (const std::length_error& e) {
        try { cs.Write("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n"); }
        catch (...) {}
        std::cerr << "request rejected: " << e.what() << '\n';
    }
    catch (const std::exception& e) {
        try { cs.Write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n"); }
        catch (...) {}
        std::cerr << "connection error: " << e.what() << '\n';
    }
    catch (...) {
        try { cs.Write("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n"); }
        catch (...) {}
        std::cerr << "connection error: unknown exception\n";
    }
}

static void server_loop() {
    ServerSocket server;
    SocketAddress addr(cfg::BASE_ADDR, cfg::PORT);
    server.Bind(addr);
    server.Listen();
    std::cout << "listening on " << cfg::BASE_ADDR << ':' << cfg::PORT << '\n';

    for (;;) {
        SocketAddress client;
        ConnectedSocket cs;
        try {
            cs = server.Accept(client);
        } catch (const std::exception& e) {
            std::cerr << "accept failed: " << e.what() << '\n';
            continue;
        }

        pid_t pid = ::fork();
        if (pid == 0) {
            server.Close();                 
            try {
                process_connection(std::move(cs), client);
            } catch (...) {
                std::cerr << "child: unhandled exception\n";
                _exit(1);
            }
            _exit(0);
        }
    }
}

static void install_signal_handlers() {
    struct sigaction sa{};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);    

    struct sigaction chld{};
    chld.sa_handler = [](int){
        while (::waitpid(-1, nullptr, WNOHANG) > 0) {}   
    };
    sigemptyset(&chld.sa_mask);
    chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &chld, nullptr);
}

[[noreturn]] static void on_terminate() {
    std::cerr << "FATAL: std::terminate called\n";
    std::abort();
}

int main() {
    std::set_terminate(on_terminate);
    install_signal_handlers();
    try {
        server_loop();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "fatal: unknown exception\n";
        return 1;
    }
    return 0;
}
