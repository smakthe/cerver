#ifndef MODEL_SETUP_H
#define MODEL_SETUP_H

#include "../database/application/orm.h"

// Register all models with the ORM
void register_all_models();

// Function to register a specific model with the ORM
Model* register_model(const char* model_name, Field* fields, int field_count);

// Find a previously registered model by name. Returns NULL if not found.
Model* find_model_by_name(const char* model_name);

#endif