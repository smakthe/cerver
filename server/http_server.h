#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

// HTTP request structure
typedef struct {
    char method[10];       // GET, POST, PUT, DELETE, PATCH
    char path[256];        // URL path
    char version[10];      // HTTP version
    char **headers;        // Array of headers
    int header_count;      // Number of headers
    char *body;            // Request body (if any)
    int body_length;       // Length of body data
    char query_string[256]; // Query parameters
} HttpRequest;

// HTTP response structure
typedef struct {
    char status[30];       // Status code and message
    char **headers;        // Array of headers
    int header_count;      // Number of headers
    char *body;            // Response body
    int body_length;       // Length of body data
    char content_type[50]; // Content type header value
} HttpResponse;

// Function to initialize a new HTTP response
HttpResponse* create_response();

// Function to free an HTTP response
void free_response(HttpResponse *response);

// Function to parse HTTP request
HttpRequest* parse_request(char *buffer, int buffer_size);

// Function to free an HTTP request
void free_request(HttpRequest *request);

// Function to add header to response
void add_response_header(HttpResponse *response, const char *name, const char *value);

// Function to handle client connection
void *handle_request(void *client_socket);

// Function to send HTTP response
void send_response(int client_socket, HttpResponse *response);

// Function to send a simple text response (convenience method)
void send_simple_response(int client_socket, const char *status, const char *content_type, const char *body);

// Function to send a JSON response (convenience method)
void send_json_response(int client_socket, const char *status, const char *body);

// Function to extract parameter from URL path
char* extract_path_parameter(const char *path, const char *pattern, const char *param_name);

// URL parameter structure
typedef struct {
    char *name;
    char *value;
} UrlParam;

// Function to parse query string into parameters
UrlParam** parse_query_string(const char *query_string, int *param_count);

// Function to free URL parameters
void free_url_params(UrlParam **params, int param_count);

// Function to start the HTTP server
void start_server(const char *port);

// Typedef for a route handler function
typedef void (*RouteHandler)(HttpRequest *request, HttpResponse *response);

// Register a route with the server
void register_route(const char *method, const char *pattern, RouteHandler handler);

// Initialize the routing system
void init_router();

// Route incoming request to the appropriate handler
void route_request(HttpRequest *request, HttpResponse *response);

#endif