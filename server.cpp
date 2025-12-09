#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define PORT 1235
#define BACKLOG 5
#define BUFSIZE 4096
#define BASE_ADDR "127.0.0.1"
#define SERVER_ADDR "SERVER_ADDR=127.0.0.1"
#define SERVER_PORT "SERVER_PORT=1235"
#define SERVER_PROTOCOL "SERVER_PROTOCOL=HTTP/1.0"
#define QUERY_STRING "QUERY_STRING="
#define SCRIPT_NAME "SCRIPT_NAME="
#define CONTENT_TYPE "CONTENT_TYPE=text/plain"
#define ERROR_PAGE "404.html"

class SocketAddress
{
    struct sockaddr_in saddr_;

public:
    SocketAddress()
    {
        saddr_.sin_family = AF_INET;
        saddr_.sin_port = htons(PORT);
        saddr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    SocketAddress(const char *ip, short port)
    {
        saddr_.sin_family = AF_INET;
        saddr_.sin_port = htons(PORT);
        saddr_.sin_addr.s_addr = inet_addr(ip);
    }

    int GetAdrLen() const { return sizeof(saddr_); }

    struct sockaddr *GetAddr() const
    {
        return (sockaddr *)&saddr_;
    }
};

class Socket
{
protected:
    int sd_;

    explicit Socket(int sd) : sd_(sd) {}

public:
    Socket() { sd_ = socket(AF_INET, SOCK_STREAM, 0); }

    ~Socket()
    {
        close(sd_);
    }

    void Shutdown()
    {
        shutdown(sd_, SHUT_RDWR);
    }
};

class ServerSocket : public Socket
{
public:
    ServerSocket() : Socket() {}

    void Bind(const SocketAddress &server_addr)
    {
        int bind_result = bind(sd_, server_addr.GetAddr(), server_addr.GetAdrLen());
        if (bind_result < 0)
        {
            cerr << "errr with bind" << endl;
            exit(1);
        }
    }

    void Listen()
    {
        int listen_result = listen(sd_, BACKLOG);
        if (listen_result < 0)
        {
            cerr << "errr with listen" << endl;
            exit(1);
        }
    }

    int Accept(SocketAddress &client_addr)
    {
        int len = client_addr.GetAdrLen();
        int client_sd = accept(sd_, (struct sockaddr *)&client_addr, (socklen_t *)&len);
        if (client_sd < 0)
        {
            cerr << "errr with accept" << endl;
            exit(1);
        }
        return client_sd;
    }
};

class ConnectedSocket : public Socket
{
public:
    ConnectedSocket() = default;

    explicit ConnectedSocket(int sd) : Socket(sd) {}

    void Write(const string &str)
    {
        int send_result = send(sd_, str.c_str(), str.length(), 0);
        if (send_result < 0)
        {
            cerr << "errr with write (string)" << endl;
            exit(1);
        }
    }

    void Read(string &str)
    {
        char buf[BUFSIZE];
        int recv_result = recv(sd_, buf, BUFSIZE, 0);
        if (recv_result < 0)
        {
            cerr << "errr with read (string)" << errno << endl;
            exit(1);
        }

        str = buf;
    }

    void Write(const vector<uint8_t> &bytes)
    {
        int send_result = send(sd_, bytes.data(), bytes.size(), 0);
        if (send_result < 0)
        {
            cerr << "errr with write (vector)" << endl;
            exit(1);
        }
    }

    void Read(vector<uint8_t> &bytes)
    {
        char buf[BUFSIZE];
        int recv_result = recv(sd_, buf, BUFSIZE, 0);
        if (recv_result < 0)
        {
            cerr << "errr with read (vector)" << endl;
            exit(1);
        }

        bytes = vector<uint8_t>(buf, buf + recv_result);
    }
};

vector<string> split_lines(const string &str)
{
    vector<string> tmp;
    string cur_line = "";

    for (char c : str)
    {
        if (c == '\n')
        {
            tmp.push_back(cur_line);
            cur_line = "";
        }
        else
        {
            cur_line += c;
        }
    }

    if (!cur_line.empty())
    {
        tmp.push_back(cur_line);
    }

    return tmp;
}

string add_lines(const vector<string> &lines)
{
    string tmp;
    for (const auto &l : lines)
    {
        tmp += l;
        tmp += '\n';
    }

    return tmp;
}

vector<uint8_t> to_vector(int fd)
{
    vector<uint8_t> res;
    char c;
    while (read(fd, &c, sizeof(char)))
        res.push_back(c);

    return res;
}

string parse_path(const string &str)
{
    string res = "./";
    for (int i = 0; i < str.length() - 1; ++i)
    {
        if (str[i] == ' ')
        {
            while (str[i + 1] != ' ' && str[i + 1] != '\n')
            {
                res += str[i + 1];
                i++;
            }
            break;
        }
    }
    if (res == ".//")
        res = "./index.html";
    return res;
}

bool is_sgi(string str)
{
    return !(str.find('?') == -1);
}

string get_cgi_file_name(string path)
{
    string res;
    for (int i = 0; path[i] != '?'; i++)
        res += path[i];
    return res;
}

string get_cgi_req(string path)
{
    string res;
    for (int i = get_cgi_file_name(path).length() + 1; i != path.length(); i++)
        res += path[i];
    return res;
}

char **create_arr(vector<string> &v)
{
    char **env = new char *[v.size() + 1];
    for (auto i = 0; i < v.size(); ++i)
        env[i] = (char *)v[i].c_str();
    env[v.size()] = NULL;
    return env;
}

void check_error(ConnectedSocket cs)
{
    string err_str;
    switch (errno)
    {
    case EACCES:
        err_str = "HTTP/1.1 403 Forbidden\n";
        break;
    case ENETRESET:
        err_str = "HTTP/1.1 503 Service Unavailable\n";
        break;
    default:
        err_str = "HTTP/1.1 404 Not Found\n";
        break;
    }

    cout << err_str << endl;
    cs.Write(err_str);
}

void cgi_connection(string path, int cd, const SocketAddress &client_addr, ConnectedSocket cs, string request)
{
    int fd;
    int pid = fork();
    switch (pid)
    {
    case -1:
    {
        perror("System error with pid");
        exit(1);
    }
    case 0:
    {
        fd = open("log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
            cout << "Error: Can't open a new file" << endl;
        dup2(fd, 1);
        close(fd);
        string file_name = get_cgi_file_name(path);
        string query = get_cgi_req(path);
        char *argv[] = {(char *)file_name.c_str(), NULL};

        vector<string> v;
        v.push_back(request);
        v.push_back(SERVER_ADDR);
        v.push_back(SERVER_PORT);
        v.push_back(SERVER_PROTOCOL);
        v.push_back(CONTENT_TYPE);
        v.push_back(QUERY_STRING + query);
        v.push_back(SCRIPT_NAME + file_name);

        char **env = create_arr(v);

        execve(file_name.c_str(), argv, env);

        check_error(cs);

        perror("with exec");
        exit(2);
    }
    default:
    {
        int status;
        wait(&status);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            fd = open("log", O_RDONLY);
            vector<uint8_t> vect = to_vector(fd);
            cs.Write("HTTP/1.1 200 OK\0");
            cout << "HTTP/1.1 200 OK" << endl;
            string str = "\r\nVersion: HTTP/1.1\r\nContent-length: " + to_string(vect.size()) + "\r\n\r\n";
            cout << "Version: "
                 << "HTTP/1.1" << endl;
            cout << "Content-length: " << to_string(vect.size()) << endl;

            cs.Write(str);
            cs.Write(vect);
            close(fd);
            cs.Shutdown();
            cs.Shutdown();
        }
        break;
    }
    }
}

void default_connection(string path, ConnectedSocket cs)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        cout << "HTTP/1.1 404 Not Found" << endl;
        cs.Write("HTTP/1.1 404 Not Found\r");
        fd = open(ERROR_PAGE, O_RDONLY);
        if (fd < 0)
        {
            cout << "error with open page 404" << endl;
        }
    }
    else
    {
        cs.Write("HTTP/1.1 200 OK\0");
    }

    vector<uint8_t> vect = to_vector(fd);
    string str = "\r\nVersion: HTTP/1.1\r\n Content-length: " + to_string(vect.size()) + "\r\n\r\n";

    cout << "Version: HTTP/1.1" << endl;
    cout << "Content-length: " << to_string(vect.size()) << endl;

    cs.Write(str);
    cs.Write(vect);
    close(fd);
    cs.Shutdown();
}

void process_connection(int cd, const SocketAddress &client_addr)
{
    ConnectedSocket cs(cd);
    string req;
    cs.Read(req);
    vector<string> lines = split_lines(req);

    string path = parse_path(lines[0]);
    cout << "path: " << path << endl;

    if (is_sgi(path))
        cgi_connection(path, cd, client_addr, cs, req);
    else
        default_connection(path, cs);
}

void server_loop()
{
    ServerSocket server_socket;
    SocketAddress server_address(BASE_ADDR, PORT);
    server_socket.Bind(server_address);
    cout << "binded succsessfull!" << endl;
    cout << "port: " << PORT << endl;

    server_socket.Listen();
    for (;;)
    {
        SocketAddress client_addr;
        int cd = server_socket.Accept(client_addr);
        process_connection(cd, client_addr);
    }
}

int main()
{
    server_loop();
    return 0;
}
