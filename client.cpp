#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define PORT 1235
#define BACKLOG 5
#define BUFSIZE 4096
#define BASE_ADDR "127.0.0.1"

class SocketAddress {
    struct sockaddr_in saddr_; // см содержимое структуры
public:
    SocketAddress() {
        saddr_.sin_family = AF_INET;
        saddr_.sin_port = htons(PORT);
        saddr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        cout << "empty constr" << endl;
    }

    SocketAddress(const char *ip, short port) {
        saddr_.sin_family = AF_INET;
        saddr_.sin_port = htons(port);
        saddr_.sin_addr.s_addr = inet_addr(ip);
    }

    int GetAdrLen() const { return sizeof(saddr_); }

    struct sockaddr *GetAddr() const {
        return (sockaddr *) &saddr_;
    }
};

class Socket {
protected:
    int sd_;

    explicit Socket(int sd) : sd_(sd) {}

public:
    Socket() { sd_ = socket(AF_INET, SOCK_STREAM, 0); }

    ~Socket() {
        close(sd_);
    }

    void Shutdown() {
        shutdown(sd_, SHUT_RDWR);
    }

    int GetSd() const { return sd_; }
};

class ConnectedSocket : public Socket {
public:
    ConnectedSocket() = default;

    explicit ConnectedSocket(int sd) : Socket(sd) {}

    void Write(const string &str) {
        int send_result = send(sd_, str.c_str(), str.length(), 0);
        if (send_result < 0) {
            cerr << "errr with write" << endl;
            exit(1);
        }
    }

    void Read(string &str) {
        char buf[BUFSIZE];
        int recv_result = recv(sd_, buf, BUFSIZE, 0);
        if (recv_result < 0) {
            cerr << "errr with read" << endl;
            exit(1);
        }

        str = buf;
    }
};

class ClientSocket : public ConnectedSocket {
public:
    void Connect(const SocketAddress &serverAddr) {
        int connect_result = connect(sd_, serverAddr.GetAddr(), serverAddr.GetAdrLen());
        if (connect_result < 0) {
            cerr << "err with connect" << endl;
            exit(1);
        }
    }
};

class HttpHeader {
    string name_;
    string value_;

public:
    HttpHeader() : name_(" "), value_(" ") {}

    HttpHeader(const string &str, const string &val) : name_(str), value_(val) {}

    HttpHeader(const HttpHeader &copy) {
        name_ = copy.name_;
        value_ = copy.value_;
    }

    string str_uni() const { return name_ + value_; }

    int len() const { return name_.length() + value_.length(); }

    static HttpHeader pars_head(const string &str) {
        int i = 0;
        string new_name, new_value;

        if (!str.empty()) {
            while (str[i] != ' ')
                new_name += str[i++];
            new_name += '\0';

            while (i < str.size())
                new_value += str[i++];
            new_value += '\0';

            cout << "str = " << str << '\n' << "len(new_name) = " << new_name.length() << " len(new_value) = " <<
                 new_value.length() << '\n' << "--------------------------" << '\n';
        } else {
            new_name = " ";
            new_value = " ";
        }

        return HttpHeader(new_name, new_value);
    }
};

class HttpRequest {
    vector<string> lines_;

public:
    HttpRequest() {
        lines_ = {"GET cgi-bin/test_inter.txt?name=dim&surname=tsarenov&mail=dim_tsar HTTP/1.1\r\0"};
    }

    string str_uni() const {
        string tmp;
        for (int i = 0; i < lines_.size(); i++)
            tmp += lines_[i];
        return tmp;
    }
};

class HttpAns {
    HttpHeader ans_;
    HttpHeader *other_;
    string body_;
    int len_;

public:
    HttpAns(vector<string> lines) {
        ans_ = HttpHeader::pars_head(lines[0]);
        other_ = new HttpHeader[lines.size() - 1];
        int i;

        for (i = 1; i < lines.size(); i++) {
            other_[i - 1] = HttpHeader::pars_head(lines[i]);
            if ((lines[i]).empty()) {
                body_ = lines[i + 1];
                break;
            }
        }
        len_ = i;
    }

    ~HttpAns() {
        delete[] other_;
        cout << "http_ans destr" << endl;
    }

    void print() const {
        cout << "ans_: " << ans_.str_uni() << endl;
        for (int i = 0; i < len_; i++) {
            cout << "other[" << i << "] : " << (other_[i]).str_uni() << endl;
            cout << (other_[i]).len() << '\n' << "----------------" << '\n';
        }
        cout << endl;
    }
};

vector<string> split_lines(const string &str) {
    vector<string> tmp;
    string cur_line = "";

    for (char c: str) {
        if (c == '\n') {
            tmp.push_back(cur_line);
            cur_line = "";
        } else {
            cur_line += c;
        }
    }

    if (!cur_line.empty()) {
        tmp.push_back(cur_line);
    }

    return tmp;
}

string add_lines(const vector<string> &lines) {
    string tmp;
    for (const auto &l: lines) {
        tmp += l;
        tmp += '\n';
    }

    return tmp;
}

void client_connection() {
    ClientSocket s;
    SocketAddress saddr(BASE_ADDR, PORT);
    s.Connect(saddr);

    HttpRequest http_req;
    string str_req = http_req.str_uni();
    s.Write(str_req);
    vector<string> lines;
    string str_ans, tmp;

    for (auto i = 0; i < 3; ++i) {
        s.Read(str_ans);
        tmp += str_ans;
    }

    lines = split_lines(tmp);
    HttpAns ans(lines);
    ans.print();
    s.Shutdown();
}

int main() {
    client_connection();
    cout << "client ending...\n";
    return 0;
}