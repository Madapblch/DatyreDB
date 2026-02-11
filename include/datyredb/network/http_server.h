#pragma once

#include "types.h"
#include <functional>
#include <map>
#include <regex>
#include <thread>
#include <atomic>
#include <mutex>

namespace datyredb::network {

// HTTP Methods
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS
};

// HTTP Request
struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    std::string body;
    std::string client_ip;
    
    std::string get_header(const std::string& name) const;
    std::string get_param(const std::string& name) const;
    
    // JSON parsing helper
    std::map<std::string, std::string> parse_json_body() const;
};

// HTTP Response
struct HttpResponse {
    int status_code{200};
    std::map<std::string, std::string> headers;
    std::string body;
    
    HttpResponse() {
        headers["Content-Type"] = "application/json";
        headers["Access-Control-Allow-Origin"] = "*";
    }
    
    static HttpResponse ok(const std::string& body);
    static HttpResponse created(const std::string& body);
    static HttpResponse bad_request(const std::string& message);
    static HttpResponse not_found(const std::string& message);
    static HttpResponse internal_error(const std::string& message);
    static HttpResponse unauthorized(const std::string& message);
    
    std::string serialize() const;
};

// Route handler
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

// HTTP Server
class HttpServer {
public:
    explicit HttpServer(uint16_t port);
    ~HttpServer();
    
    bool start();
    void stop();
    
    // Route registration
    void get(const std::string& path, RouteHandler handler);
    void post(const std::string& path, RouteHandler handler);
    void put(const std::string& path, RouteHandler handler);
    void del(const std::string& path, RouteHandler handler);
    
    // Middleware
    using Middleware = std::function<bool(HttpRequest&, HttpResponse&)>;
    void use(Middleware middleware);
    
    // CORS
    void enable_cors(const std::string& allowed_origins = "*");
    
private:
    void accept_loop();
    void handle_request(int client_fd);
    HttpRequest parse_request(const std::string& raw);
    HttpResponse route_request(const HttpRequest& req);
    
    uint16_t port_;
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    
    // Routes
    struct Route {
        std::regex pattern;
        std::vector<std::string> param_names;
        RouteHandler handler;
    };
    std::map<HttpMethod, std::vector<Route>> routes_;
    std::vector<Middleware> middlewares_;
    
    mutable std::mutex routes_mutex_;
};

} // namespace datyredb::network
