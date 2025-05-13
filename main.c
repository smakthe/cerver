#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>  // For PATH_MAX
#include "models/model_setup.h"
#include "models/scaffold_model.h"
#include "controllers/scaffold_controller.h"
#include "routes/scaffold_routes.h"
#include "server/http_server.h"
#include "database/application/orm.h"
#include "database/logical/database.h"
#include "utils/path_utils.h"

volatile sig_atomic_t running = 1;

void handle_shutdown(int sig) {
    printf("\nShutting down the server gracefully...\n");
    running = 0;
}

void scaffold_resource(const char* resource_name, const char* attributes[], const char* types[], int attr_count) {
    // Generate the scaffolding files
    scaffold_model(resource_name, attributes, types, attr_count);
    generate_controller_code(resource_name);
    generate_routes_code(resource_name);
    
    // Create field array for the model
    Field* fields = malloc(attr_count * sizeof(Field));
    if (!fields) {
        fprintf(stderr, "Error: Memory allocation failed for fields\n");
        return;
    }
    
    // Fill in field information
    for (int i = 0; i < attr_count; i++) {
        fields[i].name = strdup(attributes[i]);
        fields[i].type = strdup(types[i]);
        fields[i].is_primary = (i == 0) ? 1 : 0; // First attribute is primary key
        fields[i].is_foreign_key = 0;
        fields[i].referenced_table = NULL;
        fields[i].referenced_column = NULL;
    }
    
    // Register the model with the ORM
    Model* model = register_model(resource_name, fields, attr_count);
    if (!model) {
        fprintf(stderr, "Error: Failed to register model %s with the ORM\n", resource_name);
    }
    
    // Register the routes for this resource
    register_model_routes(resource_name);
    
    printf("Resource '%s' has been scaffolded, model registered with ORM, and routes registered.\n", resource_name);
    
    // Note: We're not freeing fields here because they're being used by the model
    // In a production app, there would be a cleanup function when shutting down
}

int main() {
    signal(SIGINT, handle_shutdown);
    
    // Initialize the database
    printf("Initializing database...\n");
    Database *db = initialize_database("cerver_db");
    if (!db) {
        fprintf(stderr, "Error: Failed to initialize database. Exiting.\n");
        return 1;
    }
    
    // Initialize model registry without default models
    printf("Initializing model system...\n");
    register_all_models();
    
    // Initialize the router system
    printf("Setting up routes...\n");
    setup_routes();
    
    // Resource scaffolding - the main purpose of this application
    printf("\n=== Resource Scaffolding ===\n");
    printf("Welcome to Cerver resource scaffolding!\n");
    printf("This will generate a model, a controller and a routes file for your resource.\n\n");
    
    char resource_name[256];
    char input[1024];

    printf("Enter the resource name (singular form, e.g., 'book', 'user', 'product'): ");
    scanf("%255s", resource_name);

    getchar(); // Consume the newline
    
    printf("\nAvailable attribute types:\n");
    printf("  int     - Integer values (e.g., id, count)\n");
    printf("  string  - Short text (e.g., name, title)\n");
    printf("  text    - Longer text (e.g., description, content)\n");
    printf("  float   - Decimal numbers (e.g., price, rating)\n");
    printf("  boolean - True/false values (e.g., is_active, in_stock)\n");
    printf("  date    - Date values (e.g., created_at, published_date)\n\n");
    
    printf("Enter attribute format:\n");
    printf("  name:type,another_name:type,...\n");
    printf("Example: id:int,title:string,price:float,description:text,published:date\n\n");
    
    printf("Enter the resource attributes: ");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = 0;

    const char *attributes[100], *types[100];
    int attr_count = 0;

    char *token = strtok(input, ",");
    while (token) {
        char *attribute = strtok(token, ":");
        char *type = strtok(NULL, ":");
        if (attribute && type) {
            attributes[attr_count] = strdup(attribute);
            types[attr_count] = strdup(type);
            attr_count++;
        }
        token = strtok(NULL, ",");
    }
    
    if (attr_count == 0) {
        printf("\nNo valid attributes found. Resource creation cancelled.\n");
    } else {
        // Convert resource name to lowercase
        char lowercase_resource[256];
        strcpy(lowercase_resource, resource_name);
        for (int i = 0; lowercase_resource[i]; i++) {
            lowercase_resource[i] = tolower(lowercase_resource[i]);
        }
        
        printf("\nCreating resource '%s' with %d attributes...\n", resource_name, attr_count);
        
        // Scaffold the resource
        scaffold_resource(resource_name, attributes, types, attr_count);
        
        // Resource creation directory path
        char resource_dir[PATH_MAX];
        char scaffolded_path[PATH_MAX];
        
        // Combine project root with scaffolded_resources path
        if (join_project_path(scaffolded_path, sizeof(scaffolded_path), "scaffolded_resources") != 0) {
            fprintf(stderr, "Error creating path to scaffolded_resources\n");
            exit(1);
        }
        
        // Create the full resource directory path
        snprintf(resource_dir, sizeof(resource_dir), "%s/%s", scaffolded_path, lowercase_resource);
        
        printf("\nResource '%s' has been successfully created!\n", resource_name);
        printf("Files created in: %s\n", resource_dir);
        printf("  - %s/%s.c\n", resource_dir, lowercase_resource);
        printf("  - %s/%s_controller.c\n", resource_dir, lowercase_resource);
        printf("  - %s/%s_routes.c\n", resource_dir, lowercase_resource);
        
        printf("\nAPI endpoints available:\n");
        printf("  GET    /%s      - List all %ss\n", lowercase_resource, lowercase_resource);
        printf("  GET    /%s/:id  - Get a specific %s by ID\n", lowercase_resource, lowercase_resource);
        printf("  POST   /%s      - Create a new %s\n", lowercase_resource, lowercase_resource);
        printf("  PATCH  /%s/:id  - Update a %s\n", lowercase_resource, lowercase_resource);
        printf("  DELETE /%s/:id  - Delete a %s\n", lowercase_resource, lowercase_resource);
    }
    
    // Clean up
    for (int i = 0; i < attr_count; i++) {
        free((void*)attributes[i]);
        free((void*)types[i]);
    }
    
    // Start the server
    printf("Starting server on port 3000...\n");
    printf("Server is now running. Press Ctrl+C to stop.\n");
    start_server("3000");
    
    // Clean up database resources (in case we reach here)
    destroy_database(db);

    return 0;
}