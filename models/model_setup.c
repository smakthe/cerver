#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model_setup.h"
#include "../database/application/orm.h"

// Global registry of models
#define MAX_MODELS 100
static Model *model_registry[MAX_MODELS];
static int model_count = 0;

// Register a model with the ORM
Model* register_model(const char* model_name, Field* fields, int field_count) {
    if (model_count >= MAX_MODELS) {
        fprintf(stderr, "Error: Maximum number of models reached\n");
        return NULL;
    }
    
    // Check if the database is initialized
    if (!global_db) {
        fprintf(stderr, "Error: Database not initialized. Call initialize_database() first.\n");
        return NULL;
    }
    
    // Create the model definition
    Model* model = define_model(model_name, fields, field_count, NULL, 0);
    if (!model) {
        fprintf(stderr, "Error: Failed to define model %s\n", model_name);
        return NULL;
    }
    
    // Store the model in the registry
    model_registry[model_count++] = model;
    
    printf("Model '%s' registered with the ORM\n", model_name);
    return model;
}

// Find a model by name
Model* find_model_by_name(const char* model_name) {
    for (int i = 0; i < model_count; i++) {
        if (strcmp(model_registry[i]->name, model_name) == 0) {
            return model_registry[i];
        }
    }
    return NULL;
}

// Register all models with the ORM
// This would normally be called at application startup
void register_all_models() {
    printf("Registering all default models with the ORM...\n");
    
    // In a real application, you would register predefined models here
    // This is a placeholder for when dynamically scaffolded models are created
    
    printf("Model registration complete.\n");
}
