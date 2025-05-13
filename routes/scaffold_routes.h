#ifndef SCAFFOLD_ROUTES_H
#define SCAFFOLD_ROUTES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../server/http_server.h"
#include "../controllers/scaffold_controller.h"

// Function to generate the routes source file for a model
void generate_routes_code(const char *model_name);

// Function to register generated routes with the HTTP server
void register_model_routes(const char *model_name);

// Helper functions for route handlers
int parse_id_from_path(const char *path);
char* extract_request_body(HttpRequest *request);

// Generic route handler prototypes
void handle_index_route(HttpRequest *request, HttpResponse *response, const char *model_name);
void handle_view_route(HttpRequest *request, HttpResponse *response, const char *model_name);
void handle_create_route(HttpRequest *request, HttpResponse *response, const char *model_name);
void handle_update_route(HttpRequest *request, HttpResponse *response, const char *model_name);
void handle_delete_route(HttpRequest *request, HttpResponse *response, const char *model_name);

// Handler registration function - used after generating model-specific routes
void setup_routes();

#endif