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
    (void)request;
    strcpy(response->content_type, "application/json");

    ControllerResult *result = indx(model_name);
    if (result && result->success && result->data) {
        response->body = strdup((char*)result->data);
    } else {
        strcpy(response->status, "500 Internal Server Error");
        response->body = strdup("{\"error\":\"failed to retrieve resources\"}");
    }
    response->body_length = strlen(response->body);
    if (result) free_controller_result(result);
}

// Handler for GET /<resource>/:id (view action)
void handle_view_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    int id = parse_id_from_path(request->path);
    strcpy(response->content_type, "application/json");

    if (id < 0) {
        strcpy(response->status, "400 Bad Request");
        response->body = strdup("{\"error\":\"invalid resource ID\"}");
        response->body_length = strlen(response->body);
        return;
    }

    ControllerResult *result = view(model_name, id);
    if (result && result->success && result->data) {
        response->body = strdup((char*)result->data);
    } else {
        strcpy(response->status, "404 Not Found");
        response->body = strdup("{\"error\":\"resource not found\"}");
    }
    response->body_length = strlen(response->body);
    if (result) free_controller_result(result);
}

// Handler for POST /<resource> (create action)
void handle_create_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    strcpy(response->content_type, "application/json");

    char *body = extract_request_body(request);
    if (!body) {
        strcpy(response->status, "400 Bad Request");
        response->body = strdup("{\"error\":\"missing request body\"}");
        response->body_length = strlen(response->body);
        return;
    }

    ControllerResult *result = create(model_name, body);
    free(body);

    if (result && result->success && result->data) {
        strcpy(response->status, "201 Created");
        response->body = strdup((char*)result->data);
    } else {
        strcpy(response->status, "422 Unprocessable Entity");
        response->body = strdup("{\"error\":\"failed to create resource\"}");
    }
    response->body_length = strlen(response->body);
    if (result) free_controller_result(result);
}

// Handler for PATCH /<resource>/:id (update action)
void handle_update_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    int id = parse_id_from_path(request->path);
    strcpy(response->content_type, "application/json");

    if (id < 0) {
        strcpy(response->status, "400 Bad Request");
        response->body = strdup("{\"error\":\"invalid resource ID\"}");
        response->body_length = strlen(response->body);
        return;
    }

    char *body = extract_request_body(request);
    if (!body) {
        strcpy(response->status, "400 Bad Request");
        response->body = strdup("{\"error\":\"missing request body\"}");
        response->body_length = strlen(response->body);
        return;
    }

    ControllerResult *result = update(model_name, id, body);
    free(body);

    if (result && result->success && result->data) {
        response->body = strdup((char*)result->data);
    } else {
        strcpy(response->status, "404 Not Found");
        response->body = strdup("{\"error\":\"resource not found\"}");
    }
    response->body_length = strlen(response->body);
    if (result) free_controller_result(result);
}

// Handler for PUT /<resource>/:id (replace action)
void handle_replace_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    int id = parse_id_from_path(request->path);
    strcpy(response->content_type, "application/json");

    if (id < 0) {
        strcpy(response->status, "400 Bad Request");
        response->body = strdup("{\"error\":\"invalid resource ID\"}");
        response->body_length = strlen(response->body);
        return;
    }

    char *body = extract_request_body(request);
    if (!body) {
        strcpy(response->status, "400 Bad Request");
        response->body = strdup("{\"error\":\"missing request body\"}");
        response->body_length = strlen(response->body);
        return;
    }

    ControllerResult *result = replace(model_name, id, body);
    free(body);

    if (result && result->success && result->data) {
        response->body = strdup((char*)result->data);
    } else {
        strcpy(response->status, "404 Not Found");
        response->body = strdup("{\"error\":\"resource not found\"}");
    }
    response->body_length = strlen(response->body);
    if (result) free_controller_result(result);
}

// Handler for DELETE /<resource>/:id (delete action)
void handle_delete_route(HttpRequest *request, HttpResponse *response, const char *model_name) {
    int id = parse_id_from_path(request->path);
    strcpy(response->content_type, "application/json");

    if (id < 0) {
        strcpy(response->status, "400 Bad Request");
        response->body = strdup("{\"error\":\"invalid resource ID\"}");
        response->body_length = strlen(response->body);
        return;
    }

    ControllerResult *result = destroy(model_name, id);
    if (result && result->success) {
        response->body = strdup("{\"message\":\"resource deleted successfully\"}");
    } else {
        strcpy(response->status, "404 Not Found");
        response->body = strdup("{\"error\":\"resource not found\"}");
    }
    response->body_length = strlen(response->body);
    if (result) free_controller_result(result);
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
    for (int i = 0; i < handler_count; i++) {
        // Index route is plural: /students, /books, etc.
        char model_path[MAX_MODEL_NAME + 3]; // +3 for '/', 's', null terminator
        snprintf(model_path, sizeof(model_path), "/%ss", route_handlers[i].model_name);

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

void replace_route_handler(HttpRequest *request, HttpResponse *response) {
    for (int i = 0; i < handler_count; i++) {
        char model_base[MAX_MODEL_NAME + 2];
        snprintf(model_base, sizeof(model_base), "/%s/", route_handlers[i].model_name);

        if (strncmp(request->path, model_base, strlen(model_base)) == 0) {
            handle_replace_route(request, response, route_handlers[i].model_name);
            return;
        }
    }

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

    // Includes
    fprintf(routes_file, "#include <stdio.h>\n");
    fprintf(routes_file, "#include <stdlib.h>\n");
    fprintf(routes_file, "#include <string.h>\n");
    fprintf(routes_file, "#include <ctype.h>\n");
    fprintf(routes_file, "#include \"../../../controllers/scaffold_controller.h\"\n");
    fprintf(routes_file, "#include \"../../../server/http_server.h\"\n");
    fprintf(routes_file, "#include \"../../../routes/scaffold_routes.h\"\n\n");

    // GET /<model> — index handler
    fprintf(routes_file, "// GET /%s — list all %s resources\n", lowercase_name, lowercase_name);
    fprintf(routes_file, "static void %s_index_handler(HttpRequest *req, HttpResponse *res) {\n", lowercase_name);
    fprintf(routes_file, "    (void)req;\n");
    fprintf(routes_file, "    ControllerResult *result = indx(\"%s\");\n", model_name);
    fprintf(routes_file, "    res->body = result && result->data ? strdup((char*)result->data) : strdup(\"[]\");\n");
    fprintf(routes_file, "    res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "    strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "    if (result) free_controller_result(result);\n");
    fprintf(routes_file, "}\n\n");

    // GET /<model>/:id — view handler
    fprintf(routes_file, "// GET /%s/:id — view a single %s\n", lowercase_name, lowercase_name);
    fprintf(routes_file, "static void %s_view_handler(HttpRequest *req, HttpResponse *res) {\n", lowercase_name);
    fprintf(routes_file, "    int id = parse_id_from_path(req->path);\n");
    fprintf(routes_file, "    if (id < 0) {\n");
    fprintf(routes_file, "        strcpy(res->status, \"400 Bad Request\");\n");
    fprintf(routes_file, "        res->body = strdup(\"{\\\"error\\\":\\\"invalid id\\\"}\");\n");
    fprintf(routes_file, "        res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "        strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "        return;\n");
    fprintf(routes_file, "    }\n");
    fprintf(routes_file, "    ControllerResult *result = view(\"%s\", id);\n", model_name);
    fprintf(routes_file, "    res->body = result && result->data ? strdup((char*)result->data) : strdup(\"{}\");\n");
    fprintf(routes_file, "    res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "    strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "    if (!result || !result->success) strcpy(res->status, \"404 Not Found\");\n");
    fprintf(routes_file, "    if (result) free_controller_result(result);\n");
    fprintf(routes_file, "}\n\n");

    // POST /<model> — create handler
    fprintf(routes_file, "// POST /%s — create a new %s\n", lowercase_name, lowercase_name);
    fprintf(routes_file, "static void %s_create_handler(HttpRequest *req, HttpResponse *res) {\n", lowercase_name);
    fprintf(routes_file, "    if (!req->body || req->body[0] != '{') {\n");
    fprintf(routes_file, "        strcpy(res->status, \"400 Bad Request\");\n");
    fprintf(routes_file, "        res->body = strdup(\"{\\\"error\\\":\\\"missing or invalid body\\\"}\");\n");
    fprintf(routes_file, "        res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "        strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "        return;\n");
    fprintf(routes_file, "    }\n");
    fprintf(routes_file, "    ControllerResult *result = create(\"%s\", req->body);\n", model_name);
    fprintf(routes_file, "    strcpy(res->status, result && result->success ? \"201 Created\" : \"422 Unprocessable Entity\");\n");
    fprintf(routes_file, "    res->body = result && result->data ? strdup((char*)result->data) : strdup(\"{}\");\n");
    fprintf(routes_file, "    res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "    strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "    if (result) free_controller_result(result);\n");
    fprintf(routes_file, "}\n\n");

    // PATCH /<model>/:id — update handler
    fprintf(routes_file, "// PATCH /%s/:id — update a %s\n", lowercase_name, lowercase_name);
    fprintf(routes_file, "static void %s_update_handler(HttpRequest *req, HttpResponse *res) {\n", lowercase_name);
    fprintf(routes_file, "    int id = parse_id_from_path(req->path);\n");
    fprintf(routes_file, "    if (id < 0) {\n");
    fprintf(routes_file, "        strcpy(res->status, \"400 Bad Request\");\n");
    fprintf(routes_file, "        res->body = strdup(\"{\\\"error\\\":\\\"invalid id\\\"}\");\n");
    fprintf(routes_file, "        res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "        strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "        return;\n");
    fprintf(routes_file, "    }\n");
    fprintf(routes_file, "    if (!req->body || req->body[0] != '{') {\n");
    fprintf(routes_file, "        strcpy(res->status, \"400 Bad Request\");\n");
    fprintf(routes_file, "        res->body = strdup(\"{\\\"error\\\":\\\"missing or invalid body\\\"}\");\n");
    fprintf(routes_file, "        res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "        strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "        return;\n");
    fprintf(routes_file, "    }\n");
    fprintf(routes_file, "    ControllerResult *result = update(\"%s\", id, req->body);\n", model_name);
    fprintf(routes_file, "    if (!result || !result->success) strcpy(res->status, \"404 Not Found\");\n");
    fprintf(routes_file, "    res->body = result && result->data ? strdup((char*)result->data) : strdup(\"{}\");\n");
    fprintf(routes_file, "    res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "    strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "    if (result) free_controller_result(result);\n");
    fprintf(routes_file, "}\n\n");

    
    // PUT /<model>/:id — replace handler
    fprintf(routes_file, "// PUT /%s/:id — fully replace a %s\n", lowercase_name, lowercase_name);
    fprintf(routes_file, "static void %s_replace_handler(HttpRequest *req, HttpResponse *res) {\n", lowercase_name);
    fprintf(routes_file, "    int id = parse_id_from_path(req->path);\n");
    fprintf(routes_file, "    if (id < 0) {\n");
    fprintf(routes_file, "        strcpy(res->status, \"400 Bad Request\");\n");
    fprintf(routes_file, "        res->body = strdup(\"{\\\"error\\\":\\\"invalid id\\\"}\");\n");
    fprintf(routes_file, "        res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "        strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "        return;\n");
    fprintf(routes_file, "    }\n");
    fprintf(routes_file, "    if (!req->body || req->body[0] != '{') {\n");
    fprintf(routes_file, "        strcpy(res->status, \"400 Bad Request\");\n");
    fprintf(routes_file, "        res->body = strdup(\"{\\\"error\\\":\\\"missing or invalid body\\\"}\");\n");
    fprintf(routes_file, "        res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "        strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "        return;\n");
    fprintf(routes_file, "    }\n");
    fprintf(routes_file, "    ControllerResult *result = replace(\"%s\", id, req->body);\n", model_name);
    fprintf(routes_file, "    if (!result || !result->success) strcpy(res->status, \"404 Not Found\");\n");
    fprintf(routes_file, "    res->body = result && result->data ? strdup((char*)result->data) : strdup(\"{}\");\n");
    fprintf(routes_file, "    res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "    strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "    if (result) free_controller_result(result);\n");
    fprintf(routes_file, "}\n\n");
    
    // DELETE /<model>/:id — destroy handler
    fprintf(routes_file, "// DELETE /%s/:id — delete a %s\n", lowercase_name, lowercase_name);
    fprintf(routes_file, "static void %s_destroy_handler(HttpRequest *req, HttpResponse *res) {\n", lowercase_name);
    fprintf(routes_file, "    int id = parse_id_from_path(req->path);\n");
    fprintf(routes_file, "    if (id < 0) {\n");
    fprintf(routes_file, "        strcpy(res->status, \"400 Bad Request\");\n");
    fprintf(routes_file, "        res->body = strdup(\"{\\\"error\\\":\\\"invalid id\\\"}\");\n");
    fprintf(routes_file, "        res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "        strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "        return;\n");
    fprintf(routes_file, "    }\n");
    fprintf(routes_file, "    ControllerResult *result = destroy(\"%s\", id);\n", model_name);
    fprintf(routes_file, "    if (!result || !result->success) strcpy(res->status, \"404 Not Found\");\n");
    fprintf(routes_file, "    res->body = result && result->data ? strdup((char*)result->data) : strdup(\"{}\");\n");
    fprintf(routes_file, "    res->body_length = strlen(res->body);\n");
    fprintf(routes_file, "    strcpy(res->content_type, \"application/json\");\n");
    fprintf(routes_file, "    if (result) free_controller_result(result);\n");
    fprintf(routes_file, "}\n\n");

    // Registration function
    fprintf(routes_file, "// Call this at startup to register all %s routes\n", lowercase_name);
    fprintf(routes_file, "void register_%s_routes() {\n", lowercase_name);
    fprintf(routes_file, "    char index_path[128], id_path[128];\n");
    // Index route is plural: /students, /books, etc.
    fprintf(routes_file, "    snprintf(index_path, sizeof(index_path), \"/%ss\");\n", lowercase_name);
    fprintf(routes_file, "    snprintf(id_path,    sizeof(id_path),    \"/%s/:id\");\n", lowercase_name);
    fprintf(routes_file, "    register_route(\"GET\",    index_path, %s_index_handler);\n", lowercase_name);
    fprintf(routes_file, "    register_route(\"GET\",    id_path,    %s_view_handler);\n", lowercase_name);
    fprintf(routes_file, "    register_route(\"POST\",   index_path, %s_create_handler);\n", lowercase_name);
    fprintf(routes_file, "    register_route(\"PATCH\",  id_path,    %s_update_handler);\n", lowercase_name);
    fprintf(routes_file, "    register_route(\"PUT\",    id_path,    %s_replace_handler);\n", lowercase_name);
    fprintf(routes_file, "    register_route(\"DELETE\", id_path,    %s_destroy_handler);\n", lowercase_name);
    fprintf(routes_file, "}\n");

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
    register_route("PUT",    "/*/*", replace_route_handler); // Wildcard for PUT replace routes
    register_route("DELETE", "/*/*", delete_route_handler); // Wildcard for DELETE destroy routes
}
