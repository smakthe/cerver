#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "scaffold_routes.h"
#include "../controllers/scaffold_controller.h"
#include "../utils/path_utils.h"

#define MAX_MODEL_NAME 100
#define MAX_ROUTE_HANDLERS 100

// Structure to hold model route handlers
typedef struct {
    char model_name[MAX_MODEL_NAME];
    RouteHandler index_handler;
    RouteHandler view_handler;
    RouteHandler create_handler;
    RouteHandler update_handler;
    RouteHandler delete_handler;
} ModelRouteHandlers;

// Global array of model route handlers
static ModelRouteHandlers route_handlers[MAX_ROUTE_HANDLERS];
static int handler_count = 0;

// Parse ID from URL path
int parse_id_from_path(const char *path) {
    if (!path) return -1;
    
    // Find the last '/' in the path
    const char *id_start = strrchr(path, '/');
    if (!id_start) return -1;
    
    // Skip the '/' character
    id_start++;
    
    // Check if the ID is numeric
    if (!isdigit(*id_start)) return -1;
    
    return atoi(id_start);
}

// Extract JSON body from request
char* extract_request_body(HttpRequest *request) {
    if (!request || !request->body || request->body_length <= 0) {
        return NULL;
    }
    
    // Just return a copy of the body
    return strdup(request->body);
}

// Handler for GET /<resource> (index action)
void handle_index_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    // Set response type to JSON
    strcpy(response->content_type, "application/json");
    
    // Build JSON prefix and suffix buffers
    char *json_prefix = "{ \"status\": \"success\", \"data\": [";
    char *json_suffix = "] }";
    
    // TODO: Call controller to get actual data
    // For now, just respond with mock data
    char json_data[1024];
    snprintf(json_data, sizeof(json_data), "%s { \"id\": 1, \"name\": \"Sample %s\" } %s", 
             json_prefix, model_name, json_suffix);
             
    response->body = strdup(json_data);
    response->body_length = strlen(json_data);
    
    // Call the controller function
    indx(model_name);
}

// Handler for GET /<resource>/:id (view action)
void handle_view_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    int id = parse_id_from_path(request->path);
    if (id < 0) {
        strcpy(response->status, "400 Bad Request");
        const char *error_msg = "{ \"status\": \"error\", \"message\": \"Invalid resource ID\" }";
        response->body = strdup(error_msg);
        response->body_length = strlen(error_msg);
        strcpy(response->content_type, "application/json");
        return;
    }
    
    // Set response type to JSON
    strcpy(response->content_type, "application/json");
    
    // TODO: Call controller to get actual data
    // For now, just respond with mock data
    char json_data[1024];
    snprintf(json_data, sizeof(json_data), "{ \"status\": \"success\", \"data\": { \"id\": %d, \"name\": \"Sample %s %d\" } }", 
             id, model_name, id);
             
    response->body = strdup(json_data);
    response->body_length = strlen(json_data);
    
    // Call the controller function
    view(model_name, id);
}

// Handler for POST /<resource> (create action)
void handle_create_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    char *body = extract_request_body(request);
    if (!body) {
        strcpy(response->status, "400 Bad Request");
        const char *error_msg = "{ \"status\": \"error\", \"message\": \"Missing request body\" }";
        response->body = strdup(error_msg);
        response->body_length = strlen(error_msg);
        strcpy(response->content_type, "application/json");
        return;
    }
    
    // Set response type to JSON
    strcpy(response->content_type, "application/json");
    strcpy(response->status, "201 Created");
    
    // TODO: Call controller to create resource
    // For now, just respond with mock data
    char json_data[1024];
    snprintf(json_data, sizeof(json_data), "{ \"status\": \"success\", \"data\": { \"id\": 123, \"message\": \"Created new %s\" } }", 
             model_name);
             
    response->body = strdup(json_data);
    response->body_length = strlen(json_data);
    
    // Call the controller function
    create(model_name, body);
    
    free(body);
}

// Handler for PATCH /<resource>/:id (update action)
void handle_update_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    int id = parse_id_from_path(request->path);
    if (id < 0) {
        strcpy(response->status, "400 Bad Request");
        const char *error_msg = "{ \"status\": \"error\", \"message\": \"Invalid resource ID\" }";
        response->body = strdup(error_msg);
        response->body_length = strlen(error_msg);
        strcpy(response->content_type, "application/json");
        return;
    }
    
    char *body = extract_request_body(request);
    if (!body) {
        strcpy(response->status, "400 Bad Request");
        const char *error_msg = "{ \"status\": \"error\", \"message\": \"Missing request body\" }";
        response->body = strdup(error_msg);
        response->body_length = strlen(error_msg);
        strcpy(response->content_type, "application/json");
        return;
    }
    
    // Set response type to JSON
    strcpy(response->content_type, "application/json");
    
    // TODO: Call controller to update resource
    // For now, just respond with mock data
    char json_data[1024];
    snprintf(json_data, sizeof(json_data), "{ \"status\": \"success\", \"data\": { \"id\": %d, \"message\": \"Updated %s %d\" } }", 
             id, model_name, id);
             
    response->body = strdup(json_data);
    response->body_length = strlen(json_data);
    
    // Call the controller function
    update(model_name, id, body);
    
    free(body);
}

// Handler for DELETE /<resource>/:id (delete action)
void handle_delete_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    int id = parse_id_from_path(request->path);
    if (id < 0) {
        strcpy(response->status, "400 Bad Request");
        const char *error_msg = "{ \"status\": \"error\", \"message\": \"Invalid resource ID\" }";
        response->body = strdup(error_msg);
        response->body_length = strlen(error_msg);
        strcpy(response->content_type, "application/json");
        return;
    }
    
    // Set response type to JSON
    strcpy(response->content_type, "application/json");
    
    // TODO: Call controller to delete resource
    // For now, just respond with mock data
    char json_data[1024];
    snprintf(json_data, sizeof(json_data), "{ \"status\": \"success\", \"message\": \"Deleted %s with ID %d\" }", 
             model_name, id);
             
    response->body = strdup(json_data);
    response->body_length = strlen(json_data);
    
    // Call the controller function
    destroy(model_name, id);
}

// Closure implementation for HTTP route handlers
// These wrapper functions allow us to pass the model name to the handler function

struct IndexRouteContext {
    const char *model_name;
};

struct ViewRouteContext {
    const char *model_name;
};

struct CreateRouteContext {
    const char *model_name;
};

struct UpdateRouteContext {
    const char *model_name;
};

struct DeleteRouteContext {
    const char *model_name;
};

void index_route_handler(HttpRequest *request, HttpResponse *response) {
    // Extract the model name from context (stored in route_handlers)
    for (int i = 0; i < handler_count; i++) {
        char model_path[MAX_MODEL_NAME + 2]; // +2 for '/' and null terminator
        snprintf(model_path, sizeof(model_path), "/%s", route_handlers[i].model_name);
        
        if (strcmp(request->path, model_path) == 0) {
            handle_index_route(request, response, route_handlers[i].model_name);
            return;
        }
    }
    
    // If no match, return 404
    strcpy(response->status, "404 Not Found");
    const char *error_msg = "{ \"status\": \"error\", \"message\": \"Resource not found\" }";
    response->body = strdup(error_msg);
    response->body_length = strlen(error_msg);
    strcpy(response->content_type, "application/json");
}

void view_route_handler(HttpRequest *request, HttpResponse *response) {
    for (int i = 0; i < handler_count; i++) {
        char model_base[MAX_MODEL_NAME + 1]; // +1 for '/' and null terminator
        snprintf(model_base, sizeof(model_base), "/%s/", route_handlers[i].model_name);
        
        if (strncmp(request->path, model_base, strlen(model_base)) == 0) {
            handle_view_route(request, response, route_handlers[i].model_name);
            return;
        }
    }
    
    // If no match, return 404
    strcpy(response->status, "404 Not Found");
    const char *error_msg = "{ \"status\": \"error\", \"message\": \"Resource not found\" }";
    response->body = strdup(error_msg);
    response->body_length = strlen(error_msg);
    strcpy(response->content_type, "application/json");
}

void create_route_handler(HttpRequest *request, HttpResponse *response) {
    for (int i = 0; i < handler_count; i++) {
        char model_path[MAX_MODEL_NAME + 2]; // +2 for '/' and null terminator
        snprintf(model_path, sizeof(model_path), "/%s", route_handlers[i].model_name);
        
        if (strcmp(request->path, model_path) == 0) {
            handle_create_route(request, response, route_handlers[i].model_name);
            return;
        }
    }
    
    // If no match, return 404
    strcpy(response->status, "404 Not Found");
    const char *error_msg = "{ \"status\": \"error\", \"message\": \"Resource not found\" }";
    response->body = strdup(error_msg);
    response->body_length = strlen(error_msg);
    strcpy(response->content_type, "application/json");
}

void update_route_handler(HttpRequest *request, HttpResponse *response) {
    for (int i = 0; i < handler_count; i++) {
        char model_base[MAX_MODEL_NAME + 1]; // +1 for '/' and null terminator
        snprintf(model_base, sizeof(model_base), "/%s/", route_handlers[i].model_name);
        
        if (strncmp(request->path, model_base, strlen(model_base)) == 0) {
            handle_update_route(request, response, route_handlers[i].model_name);
            return;
        }
    }
    
    // If no match, return 404
    strcpy(response->status, "404 Not Found");
    const char *error_msg = "{ \"status\": \"error\", \"message\": \"Resource not found\" }";
    response->body = strdup(error_msg);
    response->body_length = strlen(error_msg);
    strcpy(response->content_type, "application/json");
}

void delete_route_handler(HttpRequest *request, HttpResponse *response) {
    for (int i = 0; i < handler_count; i++) {
        char model_base[MAX_MODEL_NAME + 1]; // +1 for '/' and null terminator
        snprintf(model_base, sizeof(model_base), "/%s/", route_handlers[i].model_name);
        
        if (strncmp(request->path, model_base, strlen(model_base)) == 0) {
            handle_delete_route(request, response, route_handlers[i].model_name);
            return;
        }
    }
    
    // If no match, return 404
    strcpy(response->status, "404 Not Found");
    const char *error_msg = "{ \"status\": \"error\", \"message\": \"Resource not found\" }";
    response->body = strdup(error_msg);
    response->body_length = strlen(error_msg);
    strcpy(response->content_type, "application/json");
}

// Register a model with its routes
void register_model_routes(const char *model_name) {
    if (handler_count >= MAX_ROUTE_HANDLERS) {
        fprintf(stderr, "Maximum route handler limit reached\n");
        return;
    }
    
    // Store the model name
    strncpy(route_handlers[handler_count].model_name, model_name, MAX_MODEL_NAME - 1);
    
    // Increment handler count
    handler_count++;
}

// Convert string to lowercase
char* routes_to_lowercase(const char* str) {
    if (!str) return NULL;
    
    char* result = strdup(str);
    for (int i = 0; result[i]; i++) {
        result[i] = tolower(result[i]);
    }
    return result;
}

// Function to generate the routes source file for a model
void generate_routes_code(const char *model_name) {
    // Convert model name to lowercase
    char* lowercase_name = routes_to_lowercase(model_name);
    
    // Create directory for the resource if it doesn't exist
    char resource_dir[PATH_MAX];
    char scaffolded_path[PATH_MAX];
    
    // Combine project root with scaffolded_resources path
    if (join_project_path(scaffolded_path, sizeof(scaffolded_path), "scaffolded_resources") != 0) {
        fprintf(stderr, "Error creating path to scaffolded_resources\n");
        free(lowercase_name);
        return;
    }
    
    // Create the full resource directory path
    snprintf(resource_dir, sizeof(resource_dir), "%s/%s", scaffolded_path, lowercase_name);
    
    // Create the directory (mkdir -p equivalent)
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", resource_dir);
    system(mkdir_cmd);
    
    // Create the routes file inside the resource directory
    FILE *routes_file;
    char routes_filename[512];
    snprintf(routes_filename, sizeof(routes_filename), "%s/%s_routes.c", resource_dir, lowercase_name);
    
    // Open the file where routes code will be written
    routes_file = fopen(routes_filename, "w");
    if (routes_file == NULL) {
        printf("Error opening file for routes generation\n");
        free(lowercase_name);
        return;
    }
    
    printf("Creating routes file: %s\n", routes_filename);

    // Write route structure code
    fprintf(routes_file, "#include <stdio.h>\n");
    fprintf(routes_file, "#include <stdlib.h>\n");
    fprintf(routes_file, "#include <string.h>\n");
    fprintf(routes_file, "#include \"../controllers/scaffold_controller.h\"\n");
    fprintf(routes_file, "#include \"../server/http_server.h\"\n");
    fprintf(routes_file, "#include \"scaffold_routes.h\"\n\n");

    // Write function to register routes
    fprintf(routes_file, "// Function to register routes for %s\n", model_name);
    fprintf(routes_file, "void register_%s_routes() {\n", model_name);
    fprintf(routes_file, "    // Register this model with the route system\n");
    fprintf(routes_file, "    register_model_routes(\"%s\");\n\n", model_name);
    fprintf(routes_file, "    // Register routes with the HTTP server\n");
    fprintf(routes_file, "    // GET /%s - List all %s\n", model_name, model_name);
    fprintf(routes_file, "    // GET /%s/:id - View a single %s\n", model_name, model_name);
    fprintf(routes_file, "    // POST /%s - Create a new %s\n", model_name, model_name);
    fprintf(routes_file, "    // PATCH /%s/:id - Update a %s\n", model_name, model_name);
    fprintf(routes_file, "    // DELETE /%s/:id - Delete a %s\n", model_name, model_name);
    fprintf(routes_file, "}\n");

    // Close the file
    fclose(routes_file);

    printf("Routes code generated for %s.\n", model_name);
}

// Function to setup all routes with the HTTP server
void setup_routes() {
    // Register all route handlers with the HTTP server
    register_route("GET", "/*", index_route_handler); // Wildcard for GET index routes
    register_route("GET", "/*/*", view_route_handler); // Wildcard for GET view routes
    register_route("POST", "/*", create_route_handler); // Wildcard for POST create routes
    register_route("PATCH", "/*/*", update_route_handler); // Wildcard for PATCH update routes
    register_route("DELETE", "/*/*", delete_route_handler); // Wildcard for DELETE destroy routes
}
