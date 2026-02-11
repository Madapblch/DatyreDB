#include "datyredb/network/http_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <sstream>
#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>

namespace datyredb::network {

HttpServer::HttpServer(uint16_t port) : port_(port) {}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) return false;
    
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd_);
        return false;
    }
    
    if (listen(server_fd_, SOMAXCONN) < 0) {
        close(server_fd_);
        return false;
    }
    
    running_ = true;
    accept_thread_ = std::thread(&HttpServer::accept_loop, this);
    
    std::cout << "[INFO] HTTP Server started on port " << port_ << "\n";
    return true;
}

void HttpServer::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void HttpServer::get(const std::string& path, RouteHandler handler) {
    std::lock_guard lock(routes_mutex_);
    
    Route route;
    route.handler = std::move(handler);
    
    // Convert path params like /users/:id to regex
    std::string pattern = path;
    std::regex param_regex(":([a-zA-Z_][a-zA-Z0-9_]*)");
    std::smatch match;
    
    while (std::regex_search(pattern, match, param_regex)) {
        route.param_names.push_back(match[1].str());
        pattern = match.prefix().str() + "([^/]+)" + match.suffix().str();
    }
    
    route.pattern = std::regex("^" + pattern + "$");
    routes_[HttpMethod::GET].push_back(std::move(route));
}

void HttpServer::post(const std::string& path, RouteHandler handler) {
    std::lock_guard lock(routes_mutex_);
    
    Route route;
    route.pattern = std::regex("^" + path + "$");
    route.handler = std::move(handler);
    routes_[HttpMethod::POST].push_back(std::move(route));
}

void HttpServer::put(const std::string& path, RouteHandler handler) {
    std::lock_guard lock(routes_mutex_);
    
    Route route;
    route.pattern = std::regex("^" + path + "$");
    route.handler = std::move(handler);
    routes_[HttpMethod::PUT].push_back(std::move(route));
}

void HttpServer::del(const std::string& path, RouteHandler handler) {
    std::lock_guard lock(routes_mutex_);
    
    Route route;
    route.pattern = std::regex("^" + path + "$");
    route.handler = std::move(handler);
    routes_[HttpMethod::DELETE].push_back(std::move(route));
}

void HttpServer::use(Middleware middleware) {
    middlewares_.push_back(std::move(middleware));
}

void HttpServer::enable_cors(const std::string& allowed_origins) {
    use([allowed_origins](HttpRequest& req, HttpResponse& res) {
        res.headers["Access-Control-Allow-Origin"] = allowed_origins;
        res.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        return true;
    });
}

void HttpServer::accept_loop() {
    while (running_) {
        struct pollfd pfd{server_fd_, POLLIN, 0};
        if (poll(&pfd, 1, 1000) <= 0) continue;
        
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) continue;
        
        // Handle in thread pool (simplified - direct handling)
        std::thread([this, client_fd]() {
            handle_request(client_fd);
            close(client_fd);
        }).detach();
    }
}

void HttpServer::handle_request(int client_fd) {
    char buffer[65536];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return;
    
    buffer[bytes] = '\0';
    
    try {
        HttpRequest req = parse_request(buffer);
        HttpResponse res = route_request(req);
        
        std::string response = res.serialize();
        send(client_fd, response.c_str(), response.size(), 0);
    } catch (const std::exception& e) {
        HttpResponse res = HttpResponse::internal_error(e.what());
        std::string response = res.serialize();
        send(client_fd, response.c_str(), response.size(), 0);
    }
}

HttpRequest HttpServer::parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    
    // Parse request line
    std::string method_str, path, version;
    stream >> method_str >> path >> version;
    
    if (method_str == "GET") req.method = HttpMethod::GET;
    else if (method_str == "POST") req.method = HttpMethod::POST;
    else if (method_str == "PUT") req.method = HttpMethod::PUT;
    else if (method_str == "DELETE") req.method = HttpMethod::DELETE;
    else if (method_str == "OPTIONS") req.method = HttpMethod::OPTIONS;
    
    // Split path and query string
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        req.query_string = path.substr(query_pos + 1);
        req.path = path.substr(0, query_pos);
    } else {
        req.path = path;
    }
    
    // Parse headers
    std::string line;
    std::getline(stream, line); // consume rest of first line
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                value.erase(0, 1);
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                value.pop_back();
            }
            req.headers[name] = value;
        }
    }
    
    // Parse body
    size_t body_pos = raw.find("\r\n\r\n");
    if (body_pos != std::string::npos) {
        req.body = raw.substr(body_pos + 4);
    }
    
    return req;
}

HttpResponse HttpServer::route_request(const HttpRequest& req) {
    std::lock_guard lock(routes_mutex_);
    
    // Handle OPTIONS (CORS preflight)
    if (req.method == HttpMethod::OPTIONS) {
        HttpResponse res;
        res.status_code = 204;
        return res;
    }
    
    // Find matching route
    auto it = routes_.find(req.method);
    if (it != routes_.end()) {
        for (const auto& route : it->second) {
            std::smatch match;
            if (std::regex_match(req.path, match, route.pattern)) {
                // Extract path params
                HttpRequest mutable_req = req;
                for (size_t i = 0; i < route.param_names.size() && i + 1 < match.size(); ++i) {
                    mutable_req.params[route.param_names[i]] = match[i + 1].str();
                }
                
                // Apply middlewares
                HttpResponse res;
                for (const auto& middleware : middlewares_) {
                    if (!middleware(mutable_req, res)) {
                        return res;
                    }
                }
                
                return route.handler(mutable_req);
            }
        }
    }
    
    return HttpResponse::not_found("Route not found");
}

// Response helpers
HttpResponse HttpResponse::ok(const std::string& body) {
    HttpResponse res;
    res.status_code = 200;
    res.body = body;
    return res;
}

HttpResponse HttpResponse::created(const std::string& body) {
    HttpResponse res;
    res.status_code = 201;
    res.body = body;
    return res;
}

HttpResponse HttpResponse::bad_request(const std::string& message) {
    HttpResponse res;
    res.status_code = 400;
    res.body = R"({"error": ")" + message + R"("})";
    return res;
}

HttpResponse HttpResponse::not_found(const std::string& message) {
    HttpResponse res;
    res.status_code = 404;
    res.body = R"({"error": ")" + message + R"("})";
    return res;
}

HttpResponse HttpResponse::internal_error(const std::string& message) {
    HttpResponse res;
    res.status_code = 500;
    res.body = R"({"error": ")" + message + R"("})";
    return res;
}

HttpResponse HttpResponse::unauthorized(const std::string& message) {
    HttpResponse res;
    res.status_code = 401;
    res.body = R"({"error": ")" + message + R"("})";
    return res;
}

std::string HttpResponse::serialize() const {
    std::ostringstream oss;
    
    oss << "HTTP/1.1 " << status_code << " ";
    switch (status_code) {
        case 200: oss << "OK"; break;
        case 201: oss << "Created"; break;
        case 204: oss << "No Content"; break;
        case 400: oss << "Bad Request"; break;
        case 401: oss << "Unauthorized"; break;
        case 404: oss << "Not Found"; break;
        case 500: oss << "Internal Server Error"; break;
        default: oss << "Unknown";
    }
    oss << "\r\n";
    
    for (const auto& [name, value] : headers) {
        oss << name << ": " << value << "\r\n";
    }
    
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n";
    oss << body;
    
    return oss.str();
}

} // namespace datyredb::network
