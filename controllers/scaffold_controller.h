#ifndef SCAFFOLD_CONTROLLER_H
#define SCAFFOLD_CONTROLLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Controller result structure for better handling of results
typedef struct {
    int success;        // 1 for success, 0 for failure
    char *message;      // Success or error message
    void *data;         // Pointer to result data (if applicable)
    int data_size;      // Size of the data (if applicable)
} ControllerResult;

// Function to create a success result
ControllerResult* create_success_result(const char *message, void *data, int data_size);

// Function to create an error result
ControllerResult* create_error_result(const char *message);

// Function to free a controller result
void free_controller_result(ControllerResult *result);

// Core controller functions
// The model_name parameter allows these functions to be used generically
// for any model
ControllerResult* indx(const char *model_name);
ControllerResult* view(const char *model_name, int id);
ControllerResult* create(const char *model_name, char *data);
ControllerResult* update(const char *model_name, int id, char *data);
ControllerResult* destroy(const char *model_name, int id);

// JSON parsing utilities
char* parse_json_field(const char *json, const char *field_name);

// JSON generation utilities
char* generate_json_response(int success, const char *message, const char *data);

// Controller code generator
void generate_controller_code(const char *model_name);

#endif