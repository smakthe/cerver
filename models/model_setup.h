#ifndef MODEL_SETUP_H
#define MODEL_SETUP_H

#include "../database/application/orm.h"

// Register all models with the ORM
void register_all_models();

// Function to register a specific model with the ORM
Model* register_model(const char* model_name, Field* fields, int field_count);

#endif
