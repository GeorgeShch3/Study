#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <optional>
#include <cstring>
#include <cerrno>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

namespace cfg {
    inline constexpr short        PORT      = 1235;
    inline constexpr std::size_t  BUFSIZE   = 4096;
    inline constexpr const char*  BASE_ADDR = "127.0.0.1";
}

[[noreturn]] static void throw_errno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

class SocketAddress {
    sockaddr_in saddr_{};
public:
    SocketAddress(std::string_view ip, short port) {
        saddr_.sin_family = AF_INET;
        saddr_.sin_port   = htons(port);
        if (::inet_pton(AF_INET, ip.data(), &saddr_.sin_addr) != 1)
            throw std::invalid_argument("bad IP address");
    }
    socklen_t       Len()  const { return sizeof(saddr_); }
    const sockaddr* Addr() const { return reinterpret_cast<const sockaddr*>(&saddr_); }
};

class Socket {
protected:
    int sd_{-1};
public:
    Socket() {
        sd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sd_ < 0) throw_errno("socket");
    }
    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& o) noexcept : sd_(o.sd_) { o.sd_ = -1; }
    Socket& operator=(Socket&& o) noexcept {
        if (this != &o) { if (sd_ >= 0) ::close(sd_); sd_ = o.sd_; o.sd_ = -1; }
        return *this;
    }
    ~Socket() { if (sd_ >= 0) ::close(sd_); }
    void Shutdown() noexcept { if (sd_ >= 0) ::shutdown(sd_, SHUT_RDWR); }
    int  Fd() const noexcept { return sd_; }
};

class ClientSocket : public Socket {
public:
    void Connect(const SocketAddress& server) {
        if (::connect(sd_, server.Addr(), server.Len()) < 0) throw_errno("connect");
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

    std::string ReadAll() {
        std::string out;
        char buf[cfg::BUFSIZE];
        for (;;) {
            ssize_t n = ::recv(sd_, buf, sizeof(buf), 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw_errno("recv");
            }
            if (n == 0) break;                  
            out.append(buf, static_cast<std::size_t>(n));
        }
        return out;
    }
};

struct HttpHeader {
    std::string name;
    std::string value;

    static HttpHeader parse(std::string_view line) {
        auto colon = line.find(':');
        if (colon == std::string_view::npos)
            return {std::string(line), ""};
        std::string n(line.substr(0, colon));
        std::string v(line.substr(colon + 1));
        std::size_t start = v.find_first_not_of(' ');
        if (start != std::string::npos) v = v.substr(start);
        return {std::move(n), std::move(v)};
    }
};

class HttpResponse {
    std::string              status_;
    std::vector<HttpHeader>  headers_;
    std::string              body_;
public:
    explicit HttpResponse(std::string_view raw) {
        auto sep = raw.find("\r\n\r\n");
        std::string_view head = (sep == std::string_view::npos) ? raw : raw.substr(0, sep);
        if (sep != std::string_view::npos) body_ = std::string(raw.substr(sep + 4));

        std::istringstream iss{std::string(head)};
        std::string line;
        bool first = true;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (first) { status_ = line; first = false; continue; }
            if (line.empty()) continue;
            headers_.push_back(HttpHeader::parse(line));
        }
    }

    void Print() const {
        std::cout << "status : " << status_ << '\n';
        for (std::size_t i = 0; i < headers_.size(); ++i)
            std::cout << "header[" << i << "]: " << headers_[i].name
                      << " = " << headers_[i].value << '\n';
        if (!body_.empty())
            std::cout << "body   : " << body_ << '\n';
    }
};

static std::string build_request() {
    std::ostringstream os;
    os << "GET /cgi-bin/test_inter.txt?name=dim&surname=tsarenov&mail=dim_tsar HTTP/1.1\r\n"
       << "Host: " << cfg::BASE_ADDR << "\r\n"
       << "Connection: close\r\n\r\n";
    return os.str();
}

static void run_client() {
    ClientSocket s;
    SocketAddress addr(cfg::BASE_ADDR, cfg::PORT);
    s.Connect(addr);

    s.Write(build_request());
    std::string raw = s.ReadAll();

    HttpResponse resp(raw);
    resp.Print();
    s.Shutdown();
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);

    try {
        run_client();
    } catch (const std::exception& e) {
        std::cerr << "client error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "client error: unknown exception\n";
        return 1;
    }
    std::cout << "client ending...\n";
    return 0;
}
