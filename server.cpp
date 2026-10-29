// ═══════════════════════════════════════════════════════════════════════════════
//  TeacherDecides — Multithreaded HTTP Static File Server
//  Port: 5921 | Serves: ./public/
//  Compile: g++ -std=c++17 -O2 -pthread server.cpp -o server
// ═══════════════════════════════════════════════════════════════════════════════

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <filesystem>
#include <csignal>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace fs = std::filesystem;

// ── Configuration ────────────────────────────────────────────────────────────
static constexpr int PORT = 5921;
static const std::string PUBLIC_DIR = "./public";
static volatile sig_atomic_t g_running = 1;

// ── MIME Type Map ────────────────────────────────────────────────────────────
static const std::unordered_map<std::string, std::string> MIME_TYPES = {
    {".html", "text/html; charset=utf-8"},
    {".css",  "text/css; charset=utf-8"},
    {".js",   "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".ico",  "image/x-icon"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2","font/woff2"},
    {".ttf",  "font/ttf"},
    {".otf",  "font/otf"},
    {".mp4",  "video/mp4"},
    {".webm", "video/webm"},
    {".txt",  "text/plain; charset=utf-8"},
    {".xml",  "application/xml"},
};

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string get_mime(const std::string& path) {
    auto ext_pos = path.rfind('.');
    if (ext_pos != std::string::npos) {
        std::string ext = path.substr(ext_pos);
        auto it = MIME_TYPES.find(ext);
        if (it != MIME_TYPES.end()) return it->second;
    }
    return "application/octet-stream";
}

static std::string build_response(int status_code, const std::string& status_text,
                                   const std::string& content_type, const std::string& body) {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
         << "Content-Type: " << content_type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "\r\n"
         << body;
    return resp.str();
}

static std::string build_response_headers(int status_code, const std::string& status_text,
                                           const std::string& content_type, size_t content_length) {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
         << "Content-Type: " << content_type << "\r\n"
         << "Content-Length: " << content_length << "\r\n"
         << "Connection: close\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Cache-Control: public, max-age=86400\r\n"
         << "\r\n";
    return resp.str();
}

// ── URL decode (handles %20, %2F, etc.) ──────────────────────────────────────
static std::string url_decode(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '%' && i + 2 < src.size()) {
            int hex = 0;
            std::istringstream iss(src.substr(i + 1, 2));
            if (iss >> std::hex >> hex) {
                out += static_cast<char>(hex);
                i += 2;
            } else {
                out += src[i];
            }
        } else if (src[i] == '+') {
            out += ' ';
        } else {
            out += src[i];
        }
    }
    return out;
}

// ── Parse the first line of the HTTP request ─────────────────────────────────
static std::string parse_request_path(const std::string& request) {
    // GET /path HTTP/1.1\r\n...
    auto first_space = request.find(' ');
    if (first_space == std::string::npos) return "/";
    auto second_space = request.find(' ', first_space + 1);
    if (second_space == std::string::npos) return "/";
    std::string raw = request.substr(first_space + 1, second_space - first_space - 1);

    // Strip query string
    auto qpos = raw.find('?');
    if (qpos != std::string::npos) raw = raw.substr(0, qpos);

    return url_decode(raw);
}

// ── Prevent directory traversal attacks ──────────────────────────────────────
static bool is_safe_path(const std::string& path) {
    return path.find("..") == std::string::npos;
}

// ── Handle a single client connection ────────────────────────────────────────
static void handle_client(int client_fd) {
    // Read request (up to 8KB)
    char buffer[8192];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';
    std::string request(buffer);

    std::string url_path = parse_request_path(request);

    // Log the request
    std::cout << "[HTTP] " << url_path << std::endl;

    // Security check
    if (!is_safe_path(url_path)) {
        std::string resp = build_response(403, "Forbidden", "text/plain", "403 Forbidden");
        send(client_fd, resp.c_str(), resp.size(), 0);
        close(client_fd);
        return;
    }

    // Route: "/" → index.html
    std::string file_path = PUBLIC_DIR + url_path;
    if (url_path == "/" || url_path.back() == '/') {
        file_path = PUBLIC_DIR + "/index.html";
    }

    // Check if the file exists
    if (!fs::exists(file_path) || fs::is_directory(file_path)) {
        // Try appending .html
        if (fs::exists(file_path + ".html")) {
            file_path += ".html";
        } else {
            std::string body = "<!DOCTYPE html><html><head><title>404</title></head>"
                               "<body style='background:#23272A;color:#fff;font-family:sans-serif;"
                               "display:flex;align-items:center;justify-content:center;height:100vh;margin:0'>"
                               "<h1 style='font-size:3rem'>404 — Not Found</h1></body></html>";
            std::string resp = build_response(404, "Not Found", "text/html; charset=utf-8", body);
            send(client_fd, resp.c_str(), resp.size(), 0);
            close(client_fd);
            return;
        }
    }

    // Read and serve the file
    std::string mime = get_mime(file_path);
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::string resp = build_response(500, "Internal Server Error", "text/plain", "500 Internal Server Error");
        send(client_fd, resp.c_str(), resp.size(), 0);
        close(client_fd);
        return;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Send headers first
    std::string headers = build_response_headers(200, "OK", mime, file_size);
    send(client_fd, headers.c_str(), headers.size(), 0);

    // Stream file in chunks (efficient for large images/videos)
    constexpr size_t CHUNK_SIZE = 65536; // 64KB chunks
    char chunk[CHUNK_SIZE];
    while (file.read(chunk, CHUNK_SIZE) || file.gcount() > 0) {
        ssize_t sent = send(client_fd, chunk, file.gcount(), 0);
        if (sent < 0) break;
    }

    close(client_fd);
}

// ── Signal handler for graceful shutdown ─────────────────────────────────────
static void signal_handler(int sig) {
    std::cout << "\n[Server] Received signal " << sig << ", shutting down...\n";
    g_running = 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Main — Start listening on PORT
// ═══════════════════════════════════════════════════════════════════════════════
int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Verify public directory exists
    if (!fs::exists(PUBLIC_DIR)) {
        std::cerr << "[Server] Error: '" << PUBLIC_DIR << "' directory not found!\n";
        std::cerr << "[Server] Create it and place your index.html inside.\n";
        return 1;
    }

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "[Server] socket() failed: " << strerror(errno) << "\n";
        return 1;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Server] bind() failed on port " << PORT << ": " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    // Listen with backlog
    if (listen(server_fd, 128) < 0) {
        std::cerr << "[Server] listen() failed: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  TeacherDecides Web Server\n";
    std::cout << "  Listening on http://0.0.0.0:" << PORT << "\n";
    std::cout << "  Serving files from: " << fs::absolute(PUBLIC_DIR) << "\n";
    std::cout << "  Press Ctrl+C to stop\n";
    std::cout << "═══════════════════════════════════════════════════════\n";

    // Accept loop — spawn a thread per connection
    while (g_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            if (g_running) std::cerr << "[Server] accept() failed: " << strerror(errno) << "\n";
            continue;
        }

        // Launch detached thread for this connection
        std::thread(handle_client, client_fd).detach();
    }

    close(server_fd);
    std::cout << "[Server] Shutdown complete.\n";
    return 0;
}
