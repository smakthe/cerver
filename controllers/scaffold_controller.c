#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "scaffold_controller.h"
#include "../models/scaffold_model.h"
#include "../database/application/orm.h"

#define MAX_MODEL_NAME 100
#define MAX_JSON_SIZE 4096

// Create a success result
ControllerResult* create_success_result(const char *message, void *data, int data_size) {
    ControllerResult *result = malloc(sizeof(ControllerResult));
    if (!result) return NULL;
    
    result->success = 1;
    result->message = message ? strdup(message) : NULL;
    result->data = data;
    result->data_size = data_size;
    
    return result;
}

// Create an error result
ControllerResult* create_error_result(const char *message) {
    ControllerResult *result = malloc(sizeof(ControllerResult));
    if (!result) return NULL;
    
    result->success = 0;
    result->message = message ? strdup(message) : NULL;
    result->data = NULL;
    result->data_size = 0;
    
    return result;
}

// Free a controller result
void free_controller_result(ControllerResult *result) {
    if (!result) return;
    
    if (result->message) free(result->message);
    // Note: we don't free result->data here as it's managed by the caller
    
    free(result);
}

// Utility: Trim whitespace from a string
char* trim_whitespace(char *str) {
    if (!str) return NULL;
    
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;  // All spaces?
    
    // Trim trailing space
    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    // Write null terminator
    *(end+1) = '\0';
    
    return str;
}

// Utility: Parse a JSON field from a JSON string
char* parse_json_field(const char *json, const char *field_name) {
    if (!json || !field_name) return NULL;
    
    // Search for the field name with quotes and colon
    char field_search[256];
    snprintf(field_search, sizeof(field_search), "\"%s\":", field_name);
    
    const char *field_pos = strstr(json, field_search);
    if (!field_pos) return NULL;
    
    // Move past the field name and colon
    field_pos += strlen(field_search);
    
    // Skip whitespace
    while (isspace(*field_pos)) field_pos++;
    
    // Check if value is a string
    if (*field_pos == '"') {
        // Find the closing quote
        field_pos++; // Skip opening quote
        const char *end_quote = strchr(field_pos, '"');
        if (!end_quote) return NULL;
        
        // Extract the value
        size_t value_len = end_quote - field_pos;
        char *value = malloc(value_len + 1);
        if (!value) return NULL;
        
        strncpy(value, field_pos, value_len);
        value[value_len] = '\0';
        
        return value;
    } 
    // Check if value is a number or boolean
    else if (isdigit(*field_pos) || *field_pos == '-' || 
             strncmp(field_pos, "true", 4) == 0 || 
             strncmp(field_pos, "false", 5) == 0 ||
             strncmp(field_pos, "null", 4) == 0) {
        // Find the end of the value (comma, closing brace, or closing bracket)
        const char *end_value = field_pos;
        while (*end_value && *end_value != ',' && *end_value != '}' && *end_value != ']') {
            end_value++;
        }
        
        // Extract the value
        size_t value_len = end_value - field_pos;
        char *value = malloc(value_len + 1);
        if (!value) return NULL;
        
        strncpy(value, field_pos, value_len);
        value[value_len] = '\0';
        
        return trim_whitespace(value);
    }
    
    return NULL;
}

// Generate a JSON response
char* generate_json_response(int success, const char *message, const char *data) {
    char *json = malloc(MAX_JSON_SIZE);
    if (!json) return NULL;
    
    // Initialize with opening brace
    strcpy(json, "{");
    
    // Add success status
    sprintf(json + strlen(json), "\"status\": \"%s\"", success ? "success" : "error");
    
    // Add message if provided
    if (message) {
        sprintf(json + strlen(json), ", \"message\": \"%s\"", message);
    }
    
    // Add data if provided
    if (data) {
        sprintf(json + strlen(json), ", \"data\": %s", data);
    }
    
    // Close the JSON object
    strcat(json, "}");
    
    return json;
}

// Controller function to list all resources (index action)
ControllerResult* indx(const char *model_name) {
    printf("Listing all %s resources...\n", model_name);
    
    // TODO: Implement proper database query via ORM
    // For now, we'll return a mock result
    char data_buffer[MAX_JSON_SIZE];
    
    // Create a mock array of items
    snprintf(data_buffer, sizeof(data_buffer), 
             "[{\"id\": 1, \"name\": \"Sample %s 1\"}, {\"id\": 2, \"name\": \"Sample %s 2\"}]",
             model_name, model_name);
    
    char *data_copy = strdup(data_buffer);
    return create_success_result("Resources retrieved successfully", data_copy, strlen(data_copy));
}

// Controller function to view a single resource (view action)
ControllerResult* view(const char *model_name, int id) {
    printf("Viewing %s with ID %d...\n", model_name, id);
    
    // TODO: Implement proper database query via ORM
    // For now, we'll check if ID is valid and return a mock result
    if (id <= 0) {
        return create_error_result("Invalid resource ID");
    }
    
    char data_buffer[MAX_JSON_SIZE];
    
    // Create mock item data
    snprintf(data_buffer, sizeof(data_buffer), 
             "{\"id\": %d, \"name\": \"Sample %s %d\"}",
             id, model_name, id);
    
    char *data_copy = strdup(data_buffer);
    return create_success_result("Resource retrieved successfully", data_copy, strlen(data_copy));
}

// Controller function to create a new resource (create action)
ControllerResult* create(const char *model_name, char *data) {
    printf("Creating new %s...\n", model_name);
    
    // Check if data is valid JSON
    if (!data || data[0] != '{') {
        return create_error_result("Invalid JSON data");
    }
    
    // Parse some fields from the JSON data
    char *name = parse_json_field(data, "name");
    
    // TODO: Implement proper database insert via ORM
    // For now, we'll just create a mock response
    char data_buffer[MAX_JSON_SIZE];
    snprintf(data_buffer, sizeof(data_buffer), 
             "{\"id\": 123, \"name\": \"%s\"}",
             name ? name : "New resource");
    
    if (name) free(name);
    
    char *data_copy = strdup(data_buffer);
    return create_success_result("Resource created successfully", data_copy, strlen(data_copy));
}

// Controller function to update an existing resource (update action)
ControllerResult* update(const char *model_name, int id, char *data) {
    printf("Updating %s with ID %d...\n", model_name, id);
    
    // Check if ID is valid
    if (id <= 0) {
        return create_error_result("Invalid resource ID");
    }
    
    // Check if data is valid JSON
    if (!data || data[0] != '{') {
        return create_error_result("Invalid JSON data");
    }
    
    // Parse some fields from the JSON data
    char *name = parse_json_field(data, "name");
    
    // TODO: Implement proper database update via ORM
    // For now, we'll just create a mock response
    char data_buffer[MAX_JSON_SIZE];
    snprintf(data_buffer, sizeof(data_buffer), 
             "{\"id\": %d, \"name\": \"%s\"}",
             id, name ? name : "Updated resource");
    
    if (name) free(name);
    
    char *data_copy = strdup(data_buffer);
    return create_success_result("Resource updated successfully", data_copy, strlen(data_copy));
}

// Controller function to delete a resource (destroy action)
ControllerResult* destroy(const char *model_name, int id) {
    printf("Deleting %s with ID %d...\n", model_name, id);
    
    // Check if ID is valid
    if (id <= 0) {
        return create_error_result("Invalid resource ID");
    }
    
    // TODO: Implement proper database delete via ORM
    // For now, we'll just create a mock response
    
    return create_success_result("Resource deleted successfully", NULL, 0);
}

// Main controller function to generate the controller logic
// Convert string to lowercase
char* controller_to_lowercase(const char* str) {
    if (!str) return NULL;
    
    char* result = strdup(str);
    for (int i = 0; result[i]; i++) {
        result[i] = tolower(result[i]);
    }
    return result;
}

void generate_controller_code(const char *model_name) {
    // Convert model name to lowercase
    char* lowercase_name = controller_to_lowercase(model_name);
    
    // Create directory for the resource if it doesn't exist
    char resource_dir[256];
    snprintf(resource_dir, sizeof(resource_dir), "/Users/somak/cerver/scaffolded_resources/%s", lowercase_name);
    
    // Create the directory (mkdir -p equivalent)
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", resource_dir);
    system(mkdir_cmd);
    
    // Create the controller file inside the resource directory
    FILE *controller_file;
    char controller_filename[512];
    snprintf(controller_filename, sizeof(controller_filename), "%s/%s_controller.c", resource_dir, lowercase_name);

    controller_file = fopen(controller_filename, "w");
    if (controller_file == NULL) {
        printf("Error opening file for controller generation\n");
        free(lowercase_name);
        return;
    }
    
    printf("Creating controller file: %s\n", controller_filename);

    // Write controller structure code
    fprintf(controller_file, "#include <stdio.h>\n");
    fprintf(controller_file, "#include <stdlib.h>\n");
    fprintf(controller_file, "#include <string.h>\n");
    fprintf(controller_file, "#include <ctype.h>\n");
    fprintf(controller_file, "#include \"../controllers/scaffold_controller.h\"\n");
    fprintf(controller_file, "#include \"%s.c\"\n\n", model_name);

    // Index function to list all resources
    fprintf(controller_file, "// Controller function to list all %s resources\n", model_name);
    fprintf(controller_file, "ControllerResult* indx_%s() {\n", model_name);
    fprintf(controller_file, "    printf(\"Listing all %s resources...\\n\");\n\n", model_name);
    
    // Generate JSON response from model data
    fprintf(controller_file, "    // TODO: Implement listing all resources\n");
    fprintf(controller_file, "    char data_json[4096];\n");
    fprintf(controller_file, "    snprintf(data_json, sizeof(data_json), \"[{\\\"id\\\": 1, \\\"name\\\": \\\"Sample %s\\\"}]\");\n\n", model_name);
    
    fprintf(controller_file, "    char *data_copy = strdup(data_json);\n");
    fprintf(controller_file, "    return create_success_result(\"Resources retrieved successfully\", data_copy, strlen(data_copy));\n");
    fprintf(controller_file, "}\n\n");

    // View function to get single resource
    fprintf(controller_file, "// Controller function to view a single %s\n", model_name);
    fprintf(controller_file, "ControllerResult* view_%s(int id) {\n", model_name);
    fprintf(controller_file, "    printf(\"Viewing %s with ID %%d...\\n\", id);\n\n", model_name);
    
    fprintf(controller_file, "    // Validate ID\n");
    fprintf(controller_file, "    if (id <= 0) {\n");
    fprintf(controller_file, "        return create_error_result(\"Invalid resource ID\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Use the model's view function\n");
    fprintf(controller_file, "    %s resource;\n", model_name);
    fprintf(controller_file, "    int result = view_%s(id, &resource);\n\n", model_name);
    
    fprintf(controller_file, "    if (result != 0) {\n");
    fprintf(controller_file, "        return create_error_result(\"%s not found\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Create JSON response\n");
    fprintf(controller_file, "    char data_json[4096];\n");
    // Generate code to construct JSON from the model fields
    fprintf(controller_file, "    snprintf(data_json, sizeof(data_json), \"{\\\"id\\\": %%d, \\\"name\\\": \\\"%%s\\\"}\", id, \"Resource name\");\n\n");
    
    fprintf(controller_file, "    char *data_copy = strdup(data_json);\n");
    fprintf(controller_file, "    return create_success_result(\"Resource retrieved successfully\", data_copy, strlen(data_copy));\n");
    fprintf(controller_file, "}\n\n");

    // Create function to create a new resource
    fprintf(controller_file, "// Controller function to create a new %s\n", model_name);
    fprintf(controller_file, "ControllerResult* create_%s(char *data) {\n", model_name);
    fprintf(controller_file, "    printf(\"Creating new %s...\\n\");\n\n", model_name);
    
    fprintf(controller_file, "    // Validate JSON data\n");
    fprintf(controller_file, "    if (!data || data[0] != '{') {\n");
    fprintf(controller_file, "        return create_error_result(\"Invalid JSON data\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Parse JSON fields\n");
    fprintf(controller_file, "    char *name = parse_json_field(data, \"name\");\n\n");
    
    fprintf(controller_file, "    // Initialize model instance\n");
    fprintf(controller_file, "    %s new_resource;\n", model_name);
    fprintf(controller_file, "    // TODO: Set fields from parsed JSON\n\n");
    
    fprintf(controller_file, "    // Use the model's create function\n");
    fprintf(controller_file, "    int result = create_%s(&new_resource);\n\n", model_name);
    
    fprintf(controller_file, "    if (name) free(name);\n\n");
    
    fprintf(controller_file, "    if (result != 0) {\n");
    fprintf(controller_file, "        return create_error_result(\"Failed to create resource\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Generate success response\n");
    fprintf(controller_file, "    char data_json[4096];\n");
    fprintf(controller_file, "    snprintf(data_json, sizeof(data_json), \"{\\\"id\\\": %%d, \\\"message\\\": \\\"Resource created\\\"}\", 123);\n\n");
    
    fprintf(controller_file, "    char *data_copy = strdup(data_json);\n");
    fprintf(controller_file, "    return create_success_result(\"Resource created successfully\", data_copy, strlen(data_copy));\n");
    fprintf(controller_file, "}\n\n");

    // Update function to update a resource
    fprintf(controller_file, "// Controller function to update an existing %s\n", model_name);
    fprintf(controller_file, "ControllerResult* update_%s(int id, char *data) {\n", model_name);
    fprintf(controller_file, "    printf(\"Updating %s with ID %%d...\\n\", id);\n\n", model_name);
    
    fprintf(controller_file, "    // Validate ID\n");
    fprintf(controller_file, "    if (id <= 0) {\n");
    fprintf(controller_file, "        return create_error_result(\"Invalid resource ID\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Validate JSON data\n");
    fprintf(controller_file, "    if (!data || data[0] != '{') {\n");
    fprintf(controller_file, "        return create_error_result(\"Invalid JSON data\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Parse JSON fields\n");
    fprintf(controller_file, "    char *name = parse_json_field(data, \"name\");\n\n");
    
    fprintf(controller_file, "    // First get the existing resource\n");
    fprintf(controller_file, "    %s resource;\n", model_name);
    fprintf(controller_file, "    int get_result = view_%s(id, &resource);\n\n", model_name);
    
    fprintf(controller_file, "    if (get_result != 0) {\n");
    fprintf(controller_file, "        if (name) free(name);\n");
    fprintf(controller_file, "        return create_error_result(\"Resource not found\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Update fields from parsed JSON\n");
    fprintf(controller_file, "    // TODO: Update relevant fields\n\n");
    
    fprintf(controller_file, "    // Use the model's update function\n");
    fprintf(controller_file, "    int result = update_%s(id, &resource);\n\n", model_name);
    
    fprintf(controller_file, "    if (name) free(name);\n\n");
    
    fprintf(controller_file, "    if (result != 0) {\n");
    fprintf(controller_file, "        return create_error_result(\"Failed to update resource\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Generate success response\n");
    fprintf(controller_file, "    char data_json[4096];\n");
    fprintf(controller_file, "    snprintf(data_json, sizeof(data_json), \"{\\\"id\\\": %%d, \\\"message\\\": \\\"Resource updated\\\"}\", id);\n\n");
    
    fprintf(controller_file, "    char *data_copy = strdup(data_json);\n");
    fprintf(controller_file, "    return create_success_result(\"Resource updated successfully\", data_copy, strlen(data_copy));\n");
    fprintf(controller_file, "}\n\n");

    // Destroy function to delete a resource
    fprintf(controller_file, "// Controller function to delete a %s\n", model_name);
    fprintf(controller_file, "ControllerResult* destroy_%s(int id) {\n", model_name);
    fprintf(controller_file, "    printf(\"Deleting %s with ID %%d...\\n\", id);\n\n", model_name);
    
    fprintf(controller_file, "    // Validate ID\n");
    fprintf(controller_file, "    if (id <= 0) {\n");
    fprintf(controller_file, "        return create_error_result(\"Invalid resource ID\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    // Use the model's destroy function\n");
    fprintf(controller_file, "    int result = destroy_%s(id);\n\n", model_name);
    
    fprintf(controller_file, "    if (result != 0) {\n");
    fprintf(controller_file, "        return create_error_result(\"Failed to delete resource\");\n");
    fprintf(controller_file, "    }\n\n");
    
    fprintf(controller_file, "    return create_success_result(\"Resource deleted successfully\", NULL, 0);\n");
    fprintf(controller_file, "}\n\n");

    fclose(controller_file);
    printf("Controller code generated for %s.\n", model_name);
}