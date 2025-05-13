#include "rdbms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Configuration ---
#define MAX_REGISTERED_MODELS 50 // Maximum number of models the API can register

// --- Model Registry ---
// Simple static registry to map model names to their schema pointers.
// A more robust implementation might use a hash map.
typedef struct {
    char *name;
    Model *schema;
} ModelRegistryEntry;

static ModelRegistryEntry model_registry[MAX_REGISTERED_MODELS];
static int registered_model_count = 0;
static int db_initialized = 0; // Flag to track initialization status


// --- Internal Helper Functions ---

/**
 * @brief Finds the Model schema pointer in the registry by its name.
 * @param model_name The name of the model to find.
 * @return Pointer to the Model schema, or NULL if not found.
 */
static Model* find_model_schema_by_name(const char* model_name) {
    if (!model_name) return NULL;
    for (int i = 0; i < registered_model_count; ++i) {
        if (model_registry[i].name && strcmp(model_registry[i].name, model_name) == 0) {
            return model_registry[i].schema;
        }
    }
    return NULL; // Not found
}

/**
 * @brief Finds the index of a field within a model schema by its name.
 * @param schema Pointer to the Model schema.
 * @param field_name The name of the field to find.
 * @return The index of the field, or -1 if not found.
 */
static int find_field_index_by_name(const Model* schema, const char* field_name) {
    if (!schema || !schema->fields || !field_name) return -1;
    for (int i = 0; i < schema->field_count; ++i) {
        if (schema->fields[i].name && strcmp(schema->fields[i].name, field_name) == 0) {
            return i;
        }
    }
    return -1; // Not found
}


// --- System Initialization and Shutdown ---

int db_system_init(const char* db_name) {
    if (db_initialized) {
        fprintf(stderr, "Warning: Database system already initialized.\n");
        return 0; // Indicate already initialized (or return an error if preferred)
    }
    if (!db_name) {
        fprintf(stderr, "Error: Database name cannot be NULL for initialization.\n");
        return -1;
    }

    // Initialize the global database pointer via the ORM layer
    if (initialize_database(db_name) == NULL) {
        // initialize_database prints errors
        return -1; // Failure
    }

    // Initialize the model registry
    registered_model_count = 0;
    for(int i=0; i<MAX_REGISTERED_MODELS; ++i) {
        model_registry[i].name = NULL;
        model_registry[i].schema = NULL;
    }

    db_initialized = 1;
    printf("Database API: System initialized for database '%s'.\n", db_name);
    return 0; // Success
}

void db_system_shutdown() {
    if (!db_initialized) {
        fprintf(stderr, "Warning: Database system shutdown called but not initialized.\n");
        return;
    }

    printf("Database API: Shutting down...\n");

    // Destroy the database via the logical layer (closes files, destroys tables/indexes)
    if (global_db) {
        destroy_database(global_db);
        global_db = NULL; // Clear the global pointer
    }

    // Cleanup the model registry (free names, schemas are managed by caller or static)
    // Note: This assumes the Model structs themselves are managed elsewhere.
    // If db_define_model allocated them, they should be freed here.
    // In our current setup, define_model returns a pointer managed by the ORM layer,
    // which is cleaned up by destroy_database implicitly. We just clear the registry.
    for(int i=0; i<registered_model_count; ++i) {
        // We don't free model_registry[i].name as it points to Model->name,
        // which is freed when the Model/Table is destroyed.
        model_registry[i].name = NULL;
        model_registry[i].schema = NULL;
    }
    registered_model_count = 0;

    db_initialized = 0;
    printf("Database API: System shutdown complete.\n");
}


// --- Model Definition ---

Model* db_define_model(const char *name, Field *fields, int field_count, Association *associations, int association_count) {
    if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized. Call db_system_init() first.\n");
        return NULL;
    }
    if (registered_model_count >= MAX_REGISTERED_MODELS) {
        fprintf(stderr, "Error: Maximum number of registered models (%d) reached.\n", MAX_REGISTERED_MODELS);
        return NULL;
    }
     if (find_model_schema_by_name(name) != NULL) {
         fprintf(stderr, "Error: Model '%s' is already defined.\n", name);
         return NULL; // Or return existing model? Current behavior is to fail.
     }


    // Call the ORM layer's define_model function
    Model *new_model_schema = define_model(name, fields, field_count, associations, association_count);

    if (new_model_schema) {
        // Register the successfully created model schema
        model_registry[registered_model_count].name = new_model_schema->name; // Point to name within schema
        model_registry[registered_model_count].schema = new_model_schema;
        registered_model_count++;
        printf("Database API: Model '%s' defined and registered.\n", name);
    } else {
        // define_model already printed errors
        fprintf(stderr, "Database API: Failed to define model '%s'.\n", name);
    }

    return new_model_schema; // Return the schema pointer (or NULL on failure)
}


// --- Instance Management & Data Access ---

ModelInstance* db_create_instance(const char* model_name) {
     if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized in db_create_instance.\n");
        return NULL;
    }
    Model* schema = find_model_schema_by_name(model_name);
    if (!schema) {
        fprintf(stderr, "Error: Model schema '%s' not found in db_create_instance.\n", model_name);
        return NULL;
    }
    // Call the ORM function to create an empty instance
    return create_new_instance(schema);
}

int db_set_field(ModelInstance* instance, const char* field_name, const char* value) {
     if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized in db_set_field.\n");
        return -1;
    }
    if (!instance || !instance->model_schema) {
        fprintf(stderr, "Error: Invalid instance or schema in db_set_field.\n");
        return -1;
    }
    // Find the index corresponding to the field name
    int field_index = find_field_index_by_name(instance->model_schema, field_name);
    if (field_index == -1) {
        fprintf(stderr, "Error: Field '%s' not found in model '%s' for db_set_field.\n", field_name, instance->model_schema->name);
        return -1;
    }

    // Call the ORM helper function to set the value
    return set_instance_field(instance, field_index, value);
}

const char* db_get_field(ModelInstance* instance, const char* field_name) {
     if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized in db_get_field.\n");
        return NULL;
    }
     if (!instance || !instance->model_schema || !instance->data) {
        // fprintf(stderr, "Error: Invalid instance or schema in db_get_field.\n"); // Reduce noise maybe
        return NULL;
    }
    // Find the index corresponding to the field name
    int field_index = find_field_index_by_name(instance->model_schema, field_name);
    if (field_index == -1) {
        // fprintf(stderr, "Error: Field '%s' not found in model '%s' for db_get_field.\n", field_name, instance->model_schema->name); // Reduce noise
        return NULL;
    }
    // Return the pointer to the string in the instance's data array
    return instance->data[field_index]; // Can be NULL if the field value is NULL
}

void db_free_instance(ModelInstance* instance) {
    // Simple wrapper around the ORM function
    free_model_instance(instance);
}


// --- CRUD Operations ---

int db_save(ModelInstance* instance) {
     if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized in db_save.\n");
        return -1;
    }
    // Delegate directly to the ORM function
    return save_model_instance(instance);
}

ModelInstance* db_find_by_pk(const char* model_name, int primary_key) {
     if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized in db_find_by_pk.\n");
        return NULL;
    }
    // Find the schema associated with the model name
    Model* schema = find_model_schema_by_name(model_name);
    if (!schema) {
        fprintf(stderr, "Error: Model schema '%s' not found in db_find_by_pk.\n", model_name);
        return NULL;
    }
    // Delegate to the ORM function
    return find_model_by_primary_key(schema, primary_key);
}

int db_delete(ModelInstance* instance) {
     if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized in db_delete.\n");
        return -1;
    }
    // Delegate directly to the ORM function
    return delete_model_instance(instance);
}

// --- Utility ---

int db_compact_table(const char* model_name) {
    if (!db_initialized) {
        fprintf(stderr, "Error: Database system not initialized in db_compact_table.\n");
        return -1;
    }
    Model* schema = find_model_schema_by_name(model_name);
    if (!schema) {
        fprintf(stderr, "Error: Model schema '%s' not found in db_compact_table.\n", model_name);
        return -1;
    }
    if (!schema->table_ref) {
         fprintf(stderr, "Error: Model schema '%s' has no linked table reference for compaction.\n", model_name);
         return -1;
    }

    // Call the logical layer function
    compact_table(schema->table_ref);
    // compact_table prints its own messages. We assume success if it returns,
    // but ideally, it should return a status code.
    return 0; // Assuming success if function returns
}
