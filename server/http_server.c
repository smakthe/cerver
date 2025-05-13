#include "http_server.h"

#define PORT 3000
#define MAX_CONNECTIONS 1000
#define BUFFER_SIZE 8192
#define MAX_ROUTES 100
#define MAX_HEADERS 20

// Route structure
typedef struct {
    char method[10];
    char pattern[256];
    RouteHandler handler;
} Route;

// Global route registry
static Route *routes = NULL;
static int route_count = 0;

// Compare function for string matching with wildcards
int match_pattern(const char *pattern, const char *path) {
    // Simple matching for now - exact match or pattern ends with :id
    size_t pattern_len = strlen(pattern);
    size_t path_len = strlen(path);
    
    // Check for exact match
    if (strcmp(pattern, path) == 0) {
        return 1;
    }
    
    // Check for pattern with parameter (e.g., "/users/:id")
    const char *param_pos = strstr(pattern, "/:id");
    if (param_pos && param_pos == pattern + pattern_len - 4) {
        // Check if the path matches up to the parameter
        size_t prefix_len = param_pos - pattern;
        if (strncmp(pattern, path, prefix_len) == 0 && 
            path[prefix_len] == '/' && 
            isdigit(path[prefix_len + 1])) {
            return 1;
        }
    }
    
    return 0;
}

// Extract parameter from path
char* extract_path_parameter(const char *path, const char *pattern, const char *param_name) {
    // Find position of parameter in pattern
    char param_marker[20];
    snprintf(param_marker, sizeof(param_marker), "/:%s", param_name);
    
    const char *param_pos = strstr(pattern, param_marker);
    if (!param_pos) {
        return NULL;
    }
    
    // Calculate position in path
    size_t prefix_len = param_pos - pattern;
    const char *value_start = path + prefix_len + 1; // +1 to skip the '/'
    
    // Find end of parameter value (next '/' or end of string)
    const char *value_end = strchr(value_start, '/');
    if (!value_end) {
        value_end = value_start + strlen(value_start);
    }
    
    // Allocate and copy the value
    size_t value_len = value_end - value_start;
    char *value = malloc(value_len + 1);
    if (!value) {
        return NULL;
    }
    
    strncpy(value, value_start, value_len);
    value[value_len] = '\0';
    
    return value;
}

// Parse query string
UrlParam** parse_query_string(const char *query_string, int *param_count) {
    if (!query_string || !*query_string) {
        *param_count = 0;
        return NULL;
    }
    
    // Count parameters
    int count = 1; // At least one parameter
    for (const char *p = query_string; *p; p++) {
        if (*p == '&') count++;
    }
    
    // Allocate parameter array
    UrlParam **params = malloc(count * sizeof(UrlParam*));
    if (!params) {
        *param_count = 0;
        return NULL;
    }
    
    // Initialize count
    *param_count = 0;
    
    // Copy query string for tokenization
    char *query_copy = strdup(query_string);
    if (!query_copy) {
        free(params);
        return NULL;
    }
    
    // Parse parameters
    char *token = strtok(query_copy, "&");
    while (token) {
        // Find '=' separator
        char *sep = strchr(token, '=');
        if (sep) {
            // Allocate parameter
            UrlParam *param = malloc(sizeof(UrlParam));
            if (!param) {
                // Clean up previously allocated params
                for (int i = 0; i < *param_count; i++) {
                    free(params[i]->name);
                    free(params[i]->value);
                    free(params[i]);
                }
                free(params);
                free(query_copy);
                *param_count = 0;
                return NULL;
            }
            
            // Split name and value
            *sep = '\0';
            param->name = strdup(token);
            param->value = strdup(sep + 1);
            
            params[(*param_count)++] = param;
        }
        
        token = strtok(NULL, "&");
    }
    
    free(query_copy);
    return params;
}

// Free URL parameters
void free_url_params(UrlParam **params, int param_count) {
    if (!params) return;
    
    for (int i = 0; i < param_count; i++) {
        free(params[i]->name);
        free(params[i]->value);
        free(params[i]);
    }
    
    free(params);
}

// Initialize HTTP response
HttpResponse* create_response() {
    HttpResponse *response = malloc(sizeof(HttpResponse));
    if (!response) return NULL;
    
    // Initialize with default values
    strcpy(response->status, "200 OK");
    response->headers = malloc(MAX_HEADERS * sizeof(char*));
    if (!response->headers) {
        free(response);
        return NULL;
    }
    
    response->header_count = 0;
    response->body = NULL;
    response->body_length = 0;
    strcpy(response->content_type, "text/plain");
    
    return response;
}

// Free HTTP response
void free_response(HttpResponse *response) {
    if (!response) return;
    
    // Free headers
    for (int i = 0; i < response->header_count; i++) {
        free(response->headers[i]);
    }
    free(response->headers);
    
    // Free body if allocated
    if (response->body) {
        free(response->body);
    }
    
    free(response);
}

// Parse HTTP request
HttpRequest* parse_request(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return NULL;
    
    HttpRequest *request = malloc(sizeof(HttpRequest));
    if (!request) return NULL;
    
    // Initialize with default values
    memset(request->method, 0, sizeof(request->method));
    memset(request->path, 0, sizeof(request->path));
    memset(request->version, 0, sizeof(request->version));
    memset(request->query_string, 0, sizeof(request->query_string));
    request->headers = malloc(MAX_HEADERS * sizeof(char*));
    if (!request->headers) {
        free(request);
        return NULL;
    }
    request->header_count = 0;
    request->body = NULL;
    request->body_length = 0;
    
    // Parse request line
    char *line_end = strstr(buffer, "\r\n");
    if (!line_end) {
        free(request->headers);
        free(request);
        return NULL;
    }
    
    *line_end = '\0';
    char *request_line = buffer;
    
    // Parse method, path, version
    char *method_end = strchr(request_line, ' ');
    if (!method_end) {
        free(request->headers);
        free(request);
        return NULL;
    }
    
    *method_end = '\0';
    strncpy(request->method, request_line, sizeof(request->method) - 1);
    
    char *path_start = method_end + 1;
    char *path_end = strchr(path_start, ' ');
    if (!path_end) {
        free(request->headers);
        free(request);
        return NULL;
    }
    
    *path_end = '\0';
    
    // Check for query string
    char *query_start = strchr(path_start, '?');
    if (query_start) {
        *query_start = '\0';
        strncpy(request->path, path_start, sizeof(request->path) - 1);
        strncpy(request->query_string, query_start + 1, sizeof(request->query_string) - 1);
    } else {
        strncpy(request->path, path_start, sizeof(request->path) - 1);
    }
    
    char *version_start = path_end + 1;
    strncpy(request->version, version_start, sizeof(request->version) - 1);
    
    // Parse headers
    char *header_start = line_end + 2; // Skip \r\n
    while (1) {
        line_end = strstr(header_start, "\r\n");
        if (!line_end) break;
        
        if (header_start == line_end) {
            // Empty line, end of headers
            header_start = line_end + 2;
            break;
        }
        
        *line_end = '\0';
        
        // Only process if we haven't reached our header limit
        if (request->header_count < MAX_HEADERS) {
            request->headers[request->header_count++] = strdup(header_start);
        }
        
        header_start = line_end + 2;
    }
    
    // Parse body if present
    if (*header_start) {
        request->body_length = buffer + buffer_size - header_start;
        request->body = malloc(request->body_length + 1);
        if (request->body) {
            memcpy(request->body, header_start, request->body_length);
            request->body[request->body_length] = '\0';
        }
    }
    
    return request;
}

// Free HTTP request
void free_request(HttpRequest *request) {
    if (!request) return;
    
    // Free headers
    for (int i = 0; i < request->header_count; i++) {
        free(request->headers[i]);
    }
    free(request->headers);
    
    // Free body if allocated
    if (request->body) {
        free(request->body);
    }
    
    free(request);
}

// Add header to response
void add_response_header(HttpResponse *response, const char *name, const char *value) {
    if (!response || !name || !value || response->header_count >= MAX_HEADERS) {
        return;
    }
    
    // Allocate memory for the full header line
    size_t header_len = strlen(name) + strlen(value) + 3; // name + ': ' + value + '\0'
    char *header = malloc(header_len);
    if (!header) return;
    
    snprintf(header, header_len, "%s: %s", name, value);
    response->headers[response->header_count++] = header;
}

// Send HTTP response
void send_response(int client_socket, HttpResponse *response) {
    if (!response) return;
    
    // Create response buffer
    size_t response_size = 1024; // Start with reasonable size
    char *response_buffer = malloc(response_size);
    if (!response_buffer) return;
    
    // Format status line
    int offset = snprintf(response_buffer, response_size, "HTTP/1.1 %s\r\n", response->status);
    
    // Add Content-Type header if not empty
    if (response->content_type[0]) {
        offset += snprintf(response_buffer + offset, response_size - offset, 
                          "Content-Type: %s\r\n", response->content_type);
    }
    
    // Add Content-Length header
    offset += snprintf(response_buffer + offset, response_size - offset, 
                      "Content-Length: %d\r\n", response->body_length);
    
    // Add other headers
    for (int i = 0; i < response->header_count; i++) {
        offset += snprintf(response_buffer + offset, response_size - offset, 
                          "%s\r\n", response->headers[i]);
    }
    
    // Add the empty line to mark end of headers
    offset += snprintf(response_buffer + offset, response_size - offset, "\r\n");
    
    // Send the headers
    send(client_socket, response_buffer, offset, 0);
    
    // Send the body separately (if any)
    if (response->body && response->body_length > 0) {
        send(client_socket, response->body, response->body_length, 0);
    }
    
    free(response_buffer);
}

// Send a simple text response (convenience method)
void send_simple_response(int client_socket, const char *status, const char *content_type, const char *body) {
    HttpResponse *response = create_response();
    if (!response) return;
    
    strcpy(response->status, status);
    strcpy(response->content_type, content_type);
    
    response->body_length = strlen(body);
    response->body = strdup(body);
    
    send_response(client_socket, response);
    free_response(response);
}

// Send a JSON response (convenience method)
void send_json_response(int client_socket, const char *status, const char *body) {
    send_simple_response(client_socket, status, "application/json", body);
}

// Initialize the routing system
void init_router() {
    if (routes) return; // Already initialized
    
    routes = malloc(MAX_ROUTES * sizeof(Route));
    if (!routes) {
        fprintf(stderr, "Failed to allocate memory for routes\n");
        exit(EXIT_FAILURE);
    }
    
    route_count = 0;
}

// Register a route
void register_route(const char *method, const char *pattern, RouteHandler handler) {
    if (!routes) {
        init_router();
    }
    
    if (route_count >= MAX_ROUTES) {
        fprintf(stderr, "Maximum route limit reached\n");
        return;
    }
    
    strncpy(routes[route_count].method, method, sizeof(routes[route_count].method) - 1);
    strncpy(routes[route_count].pattern, pattern, sizeof(routes[route_count].pattern) - 1);
    routes[route_count].handler = handler;
    
    route_count++;
}

// Route the request to the appropriate handler
void route_request(HttpRequest *request, HttpResponse *response) {
    if (!routes || !request || !response) return;
    
    // Find matching route
    for (int i = 0; i < route_count; i++) {
        if (strcasecmp(routes[i].method, request->method) == 0 &&
            match_pattern(routes[i].pattern, request->path)) {
            // Found a matching route
            routes[i].handler(request, response);
            return;
        }
    }
    
    // No matching route found
    strcpy(response->status, "404 Not Found");
    const char *not_found = "404 Not Found - Resource not available";
    response->body_length = strlen(not_found);
    response->body = strdup(not_found);
}

// Handle request - main client connection handler
void *handle_request(void *client_socket) {
    int socket = (intptr_t) client_socket;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    bytes_read = read(socket, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        // Parse the request
        HttpRequest *request = parse_request(buffer, bytes_read);
        if (request) {
            // Create a response object
            HttpResponse *response = create_response();
            if (response) {
                // Route the request
                route_request(request, response);
                
                // Send the response
                send_response(socket, response);
                
                // Clean up
                free_response(response);
            } else {
                // Failed to create response
                send_simple_response(socket, "500 Internal Server Error", "text/plain", 
                                     "Internal server error: Could not create response");
            }
            
            free_request(request);
        } else {
            // Failed to parse request
            send_simple_response(socket, "400 Bad Request", "text/plain", 
                                 "Bad request: Could not parse request");
        }
    } else {
        // Failed to read request
        send_simple_response(socket, "400 Bad Request", "text/plain", 
                             "Bad request: Empty or invalid request");
    }

    close(socket);
    return NULL;
}

// Start the server
void start_server(const char *port) {
    // Initialize router
    init_router();
    
    // Set up socket
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port_number = atoi(port);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_number);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %s...\n", port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_request, (void *) (intptr_t) client_socket) != 0) {
            perror("Thread creation failed");
            close(client_socket);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_socket);
}
