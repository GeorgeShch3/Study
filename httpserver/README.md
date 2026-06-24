# HTTP Server with CGI Support

A small educational HTTP/1.1 server and client written in C++ on top of
POSIX sockets. The server serves static files and runs CGI scripts, handling
each connection in a separate process.

## Features

- Serving static files (HTML, etc.) from the working directory
- Running CGI scripts with parameters passed through `QUERY_STRING`
- Each connection handled in its own process (`fork`)
- Proper HTTP responses: `200`, `404`, `405`, `413`, `500`
- A `404.html` page when the requested file is missing
- Resilience to dropped connections and stalled clients (read timeout)

## Project Structure

| File         | Purpose                                              |
|--------------|------------------------------------------------------|
| `server.cpp` | HTTP server: accepts connections, static files, CGI  |
| `client.cpp` | Test client: sends a request and prints the response |

## Build

Requires a C++17-capable compiler (GCC or Clang) on Linux.

```sh
g++ -Wall -Wextra -O2 -o server server.cpp
g++ -Wall -Wextra -O2 -o client client.cpp
```

## Preparing Files

The server looks for files relative to the directory it is started from.
To test it, create a minimal set of files:

```sh
echo '<html><body>Hello!</body></html>' > index.html
echo '404 page' > 404.html

mkdir -p cgi-bin
printf '#!/bin/sh\necho "CGI says hi"\necho "QUERY=$QUERY_STRING"\n' > cgi-bin/test_inter.txt
chmod +x cgi-bin/test_inter.txt
```

> The CGI script **must** be executable (`chmod +x`) — the server launches it
> via `execve`, otherwise it returns `404`.

## Running

The server and client run in separate terminals.

**Terminal 1 — server:**

```sh
./server
```

On startup it prints `listening on 127.0.0.1:1235` and waits for connections.
Stop it with `Ctrl+C`.

**Terminal 2 — client:**

```sh
./client
```

The client connects, sends a request, prints the response, and exits.

## Testing with a Browser or curl

```sh
# Static file
curl http://127.0.0.1:1235/index.html

# CGI with parameters
curl "http://127.0.0.1:1235/cgi-bin/test_inter.txt?name=test&id=1"
```

In a browser: `http://127.0.0.1:1235/index.html`

## Configuration

Settings are defined as constants at the top of `server.cpp` (namespace `cfg`):

| Constant       | Value       | Description                  |
|----------------|-------------|------------------------------|
| `PORT`         | `1235`      | Listening port               |
| `BASE_ADDR`    | `127.0.0.1` | Bind address                 |
| `BACKLOG`      | `16`        | Connection queue length      |
| `MAX_REQUEST`  | `64 KB`     | Maximum request size         |
| `RECV_TIMEOUT` | `10` sec    | Client read timeout          |

## How It Works

1. The server binds to the port and listens for incoming connections.
2. For each connection it calls `fork` — a child process handles the request.
3. The request is read until the end of the headers (`\r\n\r\n`), respecting
   the timeout and size limit.
4. If the path contains `?`, it is treated as CGI: the server forks again,
   redirects the script's output to a temporary file, passes parameters through
   environment variables, and returns the result to the client.
5. Otherwise the server opens the file at the given path and serves it as a
   static file.

## Limitations

- **Path traversal:** the request path is not normalized, so a request like
  `/../../etc/passwd` may escape the working directory.
- Only the `GET` method is supported.
- No HTTPS, keep-alive, chunked transfer, or caching.
