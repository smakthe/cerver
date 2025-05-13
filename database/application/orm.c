#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "orm.h"

// --- Global Database Variable Definition ---
// Needs to be initialized by calling initialize_database() before use.
Database *global_db = NULL;

// --- Database Initialization ---

/**
 * @brief Initializes the global database pointer. Should be called once.
 * @param name Name of the database to create/connect to.
 * @return Pointer to the initialized global Database structure.
 */
Database *initialize_database(const char *name) {
    if (global_db != NULL) {
        fprintf(stderr, "Warning: Database already initialized. Returning existing instance.\n");
        return global_db;
    }
    global_db = create_database(name); // Calls the logical layer function
    if (!global_db) {
         fprintf(stderr, "FATAL: Failed to initialize database '%s'.\n", name);
         // Application might need to terminate or handle this failure.
    }
    return global_db;
}

// --- Model Schema Definition & Cleanup ---

/**
 * @brief Defines a Model schema and creates the corresponding logical table.
 * Links the ORM Model structure to the underlying Table structure.
 * @param name Name of the model/table.
 * @param fields Array of Field definitions for the model. Assumed to persist.
 * @param field_count Number of fields.
 * @param associations Array of Association definitions. Assumed to persist. (Conceptual)
 * @param association_count Number of associations.
 * @return Pointer to the created Model schema structure, or NULL on failure.
 */
Model *define_model(const char *name, Field *fields, int field_count, Association *associations, int association_count) {
    // --- Pre-conditions Check ---
    if (!global_db) {
        fprintf(stderr, "Error: Database not initialized in define_model. Call initialize_database() first.\n");
        return NULL;
    }
     if (!name || !fields || field_count <= 0) {
         fprintf(stderr, "Error: Invalid arguments provided to define_model (name='%s', field_count=%d).\n", name ? name : "NULL", field_count);
         return NULL;
     }
     // Basic check for primary key definition (at least one field should be marked PK)
     int pk_found = 0;
     for(int i=0; i<field_count; ++i) {
         if(fields[i].is_primary) {
             pk_found = 1;
             break;
         }
     }
     if (!pk_found) {
          fprintf(stderr, "Error: Model schema '%s' must define at least one primary key field.\n", name);
          return NULL;
     }


    // --- Allocate Model Structure ---
    Model *model = (Model *)malloc(sizeof(Model));
    if (!model) {
        perror("Failed to allocate memory for Model schema");
        return NULL;
    }
    // Initialize fields
    model->name = NULL;
    model->table_ref = NULL;
    model->fields = NULL; // Will point to user-provided array
    model->associations = NULL; // Will point to user-provided array


    // --- Duplicate Name & Extract Column Names ---
    model->name = strdup(name);
    if (!model->name) {
        perror("Failed to duplicate model name");
        free(model);
        return NULL;
    }

    // Create a temporary array of column name pointers for create_table
    char **column_names = (char **)malloc(field_count * sizeof(char *));
    if (!column_names) {
        perror("Failed to allocate memory for column names array");
        free(model->name);
        free(model);
        return NULL;
    }
    for (int i = 0; i < field_count; i++) {
        if (!fields[i].name) { // Validate field names
             fprintf(stderr, "Error: Field at index %d in model '%s' has NULL name.\n", i, name);
             free(column_names);
             free(model->name);
             free(model);
             return NULL;
        }
        column_names[i] = fields[i].name; // Point to names within the user-provided Field structs
    }

    // --- Create Underlying Logical Table ---
    model->table_ref = create_table(global_db, name, column_names, field_count);
    free(column_names); // Free the temporary array of pointers (not the names themselves)

    if (!model->table_ref) {
        fprintf(stderr, "Error: Failed to create logical table for model '%s'. ORM definition failed.\n", name);
        free(model->name);
        free(model);
        return NULL;
    }

    // --- Store Schema Details ---
    // Store pointers to the user-provided arrays. The ORM assumes these arrays
    // (and the Field/Association structs within them) will remain valid.
    model->fields = fields;
    model->field_count = field_count;
    model->associations = associations;
    model->association_count = association_count;

    // Initialize callbacks (if used) to NULL
    // model->before_save = NULL;
    // model->after_save = NULL;
    // ...

    return model;
}

/**
 * @brief Frees the memory allocated for the Model schema structure itself.
 * Does NOT free the underlying Table, nor the Field/Association arrays
 * that were passed into define_model.
 * @param model Pointer to the Model schema to destroy.
 */
void destroy_model_schema(Model* model) {
    if (!model) return;
    // We only allocated the Model struct and duplicated its name.
    free(model->name);
    // The table_ref is managed by the logical layer (destroy_database)
    // The fields and associations arrays are assumed to be managed externally.
    free(model);
}


// --- Model Instance Management ---

/**
 * @brief Allocates a new, empty ModelInstance structure linked to a schema.
 * Initializes data pointers to NULL and record_offset to -1.
 * @param model_schema Pointer to the Model schema this instance belongs to.
 * @return Pointer to the newly allocated ModelInstance, or NULL on failure.
 */
ModelInstance* create_new_instance(Model* model_schema) {
    if (!model_schema) {
        fprintf(stderr, "Error: Cannot create instance with NULL model schema.\n");
        return NULL;
    }

    ModelInstance* instance = (ModelInstance*)malloc(sizeof(ModelInstance));
    if (!instance) {
        perror("Failed to allocate ModelInstance structure");
        return NULL;
    }

    instance->model_schema = model_schema;
    instance->record_offset = -1; // Mark as new/unsaved initially

    // Allocate the data array (array of char pointers)
    // Use calloc to initialize all pointers to NULL
    instance->data = (char**)calloc(model_schema->field_count, sizeof(char*));
    if (!instance->data) {
        perror("Failed to allocate data array for ModelInstance");
        free(instance);
        return NULL;
    }

    return instance;
}

/**
* @brief Sets a field value in a ModelInstance by index, handling string duplication.
* @param instance Pointer to the ModelInstance.
* @param field_index Index of the field to set.
* @param value The string value to set (will be duplicated). NULL clears the field.
* @return 0 on success, -1 on failure.
*/

int set_instance_field(ModelInstance *instance, int field_index, const char *value) {
    // Validate input
    if (!instance || !instance->model_schema || !instance->data ||
        field_index < 0 || field_index >= instance->model_schema->field_count) {
        fprintf(stderr, "Error: Invalid arguments to set_instance_field (index: %d).\n", field_index);
        return -1;
    }

    // Free existing data at this index to prevent memory leaks
    free(instance->data[field_index]);
    instance->data[field_index] = NULL; // Set to NULL before potential strdup

    // If a non-NULL value is provided, duplicate it
    if (value != NULL) {
        instance->data[field_index] = strdup(value);
        if (!instance->data[field_index]) {
            perror("Failed to duplicate string in set_instance_field");
            return -1; // Memory allocation failed
        }
    }
    // If value is NULL, the field is now correctly set to NULL
    return 0; // Success
}

/**
* @brief Frees a ModelInstance structure and all the data strings it holds.
* @param instance Pointer to the ModelInstance to free.
*/

void free_model_instance(ModelInstance* instance) {
    if (!instance) return;

    // Free the individual data strings if they were allocated (strdup'd)
    if (instance->data) {
        // Check schema pointer validity before accessing field_count
        if (instance->model_schema) {
            for (int i = 0; i < instance->model_schema->field_count; i++) {
                free(instance->data[i]); // free(NULL) is safe
            }
        } else {
             fprintf(stderr, "Warning: Freeing instance with NULL schema pointer.\n");
             // Cannot determine size of data array safely. Potential memory leak if data existed.
        }
        free(instance->data); // Free the array of pointers
    }

    // Free the instance structure itself
    free(instance);
}


// --- ORM CRUD Operations ---

/**
 * @brief Helper function to find the index of the primary key field in a model schema.
 * @param model Pointer to the Model schema.
 * @return The index of the primary key field, or -1 if not found.
 */
int find_primary_key_index(Model* model) {
    if (!model || !model->fields) return -1;
    for (int i = 0; i < model->field_count; ++i) {
        if (model->fields[i].is_primary) {
            return i; // Return index of the first PK found
        }
    }
    return -1; // No primary key field found in the schema
}

/**
 * @brief Saves (inserts or updates) a ModelInstance to the database.
 * @param instance Pointer to the ModelInstance to save.
 * @return 0 on success, -1 on failure.
 */
int save_model_instance(ModelInstance *instance) {
    // --- Input Validation ---
    if (!instance || !instance->model_schema || !instance->data || !instance->model_schema->table_ref) {
        fprintf(stderr, "Error: Invalid ModelInstance or schema provided to save_model_instance.\n");
        return -1;
    }

    Model* schema = instance->model_schema;
    Table* table = schema->table_ref;

    // --- Validation Hook (Example - Implement if needed) ---
    // if (schema->validate && schema->validate(instance) != 0) {
    //     fprintf(stderr, "Validation failed for model '%s'. Save aborted.\n", schema->name);
    //     return -1; // Indicate validation failure
    // }

    // --- Identify Primary Key and Value ---
    int pk_index = find_primary_key_index(schema);
    if (pk_index == -1) {
         fprintf(stderr, "Error: Cannot save model '%s', no primary key defined in schema.\n", schema->name);
         return -1;
    }
    // Ensure the primary key field in the instance data is not NULL
    if (instance->data[pk_index] == NULL) {
         fprintf(stderr, "Error: Cannot save model '%s', primary key value is NULL.\n", schema->name);
         return -1;
    }
    // Convert primary key string to integer for logical layer functions
    // Add error checking for atoi if PK might not be a valid integer string
    int primary_key = atoi(instance->data[pk_index]);
     // Basic check: if atoi returned 0, ensure the string was actually "0"
     if (primary_key == 0 && strcmp(instance->data[pk_index], "0") != 0) {
         fprintf(stderr, "Error: Invalid integer format for primary key value '%s' in model '%s'.\n", instance->data[pk_index], schema->name);
         return -1;
     }


    // --- Callbacks (Example - Implement if needed) ---
    // Distinguish between insert and update for callbacks
    // if (instance->record_offset == -1) { // This is an INSERT operation
    //     if(schema->before_save) schema->before_save(instance); // Generic before_save
    //     // Or specific before_insert callback
    // } else { // This is an UPDATE operation
    //     if(schema->before_save) schema->before_save(instance); // Generic before_save
    //     // Or specific before_update callback
    // }


    // --- Perform Database Operation (Insert or Update) ---
    long result_offset = -1;
    if (instance->record_offset == -1) {
        // INSERT: Instance is new, call insert_row
        result_offset = insert_row(table, primary_key, instance->data);
        if (result_offset != -1) {
            instance->record_offset = result_offset; // Update instance with the new offset
            // if (schema->after_save) schema->after_save(instance); // After insert callback
            printf("Instance of '%s' (PK=%d) inserted at offset %ld.\n", schema->name, primary_key, result_offset);
        } else {
            // insert_row already printed an error
            return -1; // Insert failed
        }
    } else {
        // UPDATE: Instance exists, call update_row
        result_offset = update_row(table, primary_key, instance->data);
         if (result_offset != -1) {
              instance->record_offset = result_offset; // Update instance offset (might change)
              // if (schema->after_save) schema->after_save(instance); // After update callback
              printf("Instance of '%s' (PK=%d) updated, new offset %ld.\n", schema->name, primary_key, result_offset);
         } else {
              // update_row already printed an error
              return -1; // Update failed
         }
    }

    return 0; // Success
}

/**
 * @brief Deletes a ModelInstance from the database.
 * @param instance Pointer to the ModelInstance to delete. Must have been previously saved.
 * @return 0 on success, -1 on failure.
 */
int delete_model_instance(ModelInstance *instance) {
     // --- Input Validation ---
     if (!instance || !instance->model_schema || !instance->data || !instance->model_schema->table_ref) {
        fprintf(stderr, "Error: Invalid ModelInstance or schema provided to delete_model_instance.\n");
        return -1;
    }
     // Cannot delete an instance that hasn't been saved (no record offset)
     if (instance->record_offset == -1) {
         fprintf(stderr, "Error: Cannot delete instance of '%s' that was never saved or already deleted.\n", instance->model_schema->name);
         return -1;
     }

    Model* schema = instance->model_schema;
    Table* table = schema->table_ref;

     // --- Identify Primary Key ---
    int pk_index = find_primary_key_index(schema);
     if (pk_index == -1 || instance->data[pk_index] == NULL) {
         fprintf(stderr, "Error: Cannot determine primary key for delete operation on '%s'. Instance data invalid?\n", schema->name);
         return -1;
     }
     int primary_key = atoi(instance->data[pk_index]);
      if (primary_key == 0 && strcmp(instance->data[pk_index], "0") != 0) {
         fprintf(stderr, "Error: Invalid integer primary key value '%s' during delete of '%s'.\n", instance->data[pk_index], schema->name);
         return -1;
     }

    // --- Callbacks (Example - Implement if needed) ---
    // if (schema->before_delete) schema->before_delete(instance);

    // --- Perform Database Delete Operation ---
    if (delete_row(table, primary_key) == 0) {
        // Success
        long old_offset = instance->record_offset; // Store offset for logging
        instance->record_offset = -1; // Mark instance as no longer persisted
        // if (schema->after_delete) schema->after_delete(instance); // After delete callback
        printf("Instance of '%s' (PK=%d, old offset=%ld) deleted.\n", schema->name, primary_key, old_offset);
        return 0;
    } else {
        // delete_row already printed an error
        return -1; // Delete failed
    }
}

/**
 * @brief Finds a ModelInstance by its primary key.
 * @param model_schema Pointer to the Model schema.
 * @param primary_key The integer primary key value.
 * @return Pointer to a *newly allocated* ModelInstance, or NULL if not found/error. Caller must free.
 */
ModelInstance* find_model_by_primary_key(Model *model_schema, int primary_key) {
    // --- Input Validation ---
    if (!model_schema || !model_schema->table_ref) {
        fprintf(stderr, "Error: Invalid model schema provided to find_model_by_primary_key.\n");
        return NULL;
    }

    Table* table = model_schema->table_ref;

    // --- Perform Read Operation using Logical Layer ---
    // read_row returns a newly allocated char** array or NULL
    char **row_values = read_row(table, primary_key);

    // --- Handle Not Found or Read Error ---
    if (row_values == NULL) {
        // read_row handles logging "not found" or read errors.
        return NULL; // Return NULL to indicate not found or error
    }

    // --- Create and Populate ModelInstance ---
    // Row found, create a new ModelInstance to hold the data
    ModelInstance* instance = create_new_instance(model_schema);
    if (!instance) {
        // Allocation failed, free the data returned by read_row
        fprintf(stderr, "Error: Failed to allocate ModelInstance after finding row PK %d.\n", primary_key);
        for (int i = 0; i < model_schema->field_count; i++) free(row_values[i]);
        free(row_values);
        return NULL;
    }

    // Get the record offset (search index again - slightly inefficient, could modify read_row)
    instance->record_offset = search_key(table->primary_index, primary_key);
    if (instance->record_offset == -1) {
         // This indicates an inconsistency: read_row found data, but search_key didn't!
         fprintf(stderr, "CRITICAL INCONSISTENCY: Row data found for PK %d but key not in index!\n", primary_key);
         free_model_instance(instance); // Free the partially created instance
         for (int i = 0; i < model_schema->field_count; i++) free(row_values[i]);
         free(row_values);
         return NULL;
    }


    // Copy the data from row_values into the instance's data array
    // We need to strdup each string again, as the instance needs its own copies.
    for (int i = 0; i < model_schema->field_count; i++) {
        if (row_values[i] != NULL) {
            instance->data[i] = strdup(row_values[i]);
            if (!instance->data[i]) {
                 perror("Failed to duplicate field data during find");
                 // Cleanup partially created instance and the rest of row_values
                 free_model_instance(instance); // Frees already strdup'd data[j]
                 for (int j = i; j < model_schema->field_count; j++) free(row_values[j]); // Free remaining row_values
                 free(row_values);
                 return NULL;
            }
        } else {
             // This case shouldn't happen if read_row returned valid data, but handle defensively
             instance->data[i] = NULL;
        }
         // Free the original string returned by read_row now that we have a copy
         free(row_values[i]);
    }
    free(row_values); // Free the char** array returned by read_row

    // --- Return the Populated Instance ---
    // Caller is responsible for freeing this instance later using free_model_instance()
    return instance;
}


// --- Utility / Schema Inspection / Configuration ---

/**
 * @brief Adds foreign key metadata to a Field definition within a Model schema.
 * Note: This only updates the ORM's schema representation; it does not enforce
 * constraints at the logical or physical layer in this implementation.
 * @param model Pointer to the Model schema.
 * @param field_name Name of the field to mark as a foreign key.
 * @param referenced_table Name of the table the foreign key references.
 * @param referenced_column Name of the column in the referenced table.
 */
void add_foreign_key(Model *model, const char *field_name, const char *referenced_table, const char *referenced_column) {
     if (!model || !field_name || !referenced_table || !referenced_column) {
          fprintf(stderr, "Warning: Invalid arguments to add_foreign_key.\n");
          return;
     }
     int found = 0;
     for (int i = 0; i < model->field_count; i++) {
        if (model->fields && model->fields[i].name && strcmp(model->fields[i].name, field_name) == 0) {
            model->fields[i].is_foreign_key = 1;
            // Store pointers to the provided strings. Assumes these strings persist.
            // For safety, you might strdup these if their lifetime is uncertain.
            model->fields[i].referenced_table = (char*)referenced_table;
            model->fields[i].referenced_column = (char*)referenced_column;
            printf("ORM: Added FK metadata to %s.%s -> %s.%s\n", model->name, field_name, referenced_table, referenced_column);
            found = 1;
            break;
        }
    }
     if (!found) {
          fprintf(stderr, "Warning: Field '%s' not found in model '%s' to add foreign key.\n", field_name, model->name);
     }
}

/**
 * @brief Prints the schema definition of a model (fields, associations).
 * @param model Pointer to the Model schema to print.
 */
void print_model_schema(Model *model) {
     if (!model) {
         printf("Cannot print schema of NULL model.\n");
         return;
     }
    printf("\n--- ORM Model Schema: '%s' ---\n", model->name);
     printf("  Underlying Table: %s\n", model->table_ref ? model->table_ref->name : "!!ERROR: No Table Linked!!");
     printf("  Fields (%d):\n", model->field_count);
     if (model->fields) {
         for (int i = 0; i < model->field_count; i++) {
             Field *f = &model->fields[i];
             printf("    - %s (%s) %s %s",
                    f->name ? f->name : "??",
                    f->type ? f->type : "??",
                    f->is_primary ? "[PK]" : "",
                    f->is_foreign_key ? "[FK]" : "");
             if (f->is_foreign_key) {
                 printf(" -> %s.%s",
                        f->referenced_table ? f->referenced_table : "??",
                        f->referenced_column ? f->referenced_column : "??");
             }
             printf("\n");
         }
     } else {
          printf("    (No field definitions linked)\n");
     }
     printf("  Associations (%d):\n", model->association_count);
      if (model->associations) {
          if (model->association_count == 0) printf("    (None)\n");
          for (int i = 0; i < model->association_count; i++) {
              Association *a = &model->associations[i];
              printf("    - Type: %s, Related: %s, Key: %s\n",
                     a->type ? a->type : "??",
                     a->related_model ? a->related_model : "??",
                     a->foreign_key ? a->foreign_key : "??");
          }
      } else {
           printf("    (No association definitions linked)\n");
      }
     printf("--- End Schema: '%s' ---\n\n", model->name);
}

/**
 * @brief Prints the data currently held within a ModelInstance.
 * @param instance Pointer to the ModelInstance to print.
 */
void print_model_instance(ModelInstance *instance) {
     if (!instance || !instance->model_schema) {
         printf("Cannot print invalid instance or instance with NULL schema.\n");
         return;
     }
     Model* schema = instance->model_schema;
     printf("\n--- ORM Model Instance: '%s' ---\n", schema->name);
     printf("  Record Offset: %ld %s\n", instance->record_offset, (instance->record_offset == -1) ? "(New/Unsaved/Deleted)" : "(Persisted)");
     printf("  Data Fields (%d):\n", schema->field_count);
     if (instance->data && schema->fields) {
         for(int i=0; i < schema->field_count; ++i) {
             printf("    - %s (%s): \"%s\"\n",
                    schema->fields[i].name ? schema->fields[i].name : "??",
                    schema->fields[i].type ? schema->fields[i].type : "??",
                    instance->data[i] ? instance->data[i] : "(NULL)"); // Print data string or "(NULL)"
         }
     } else {
          printf("    (Instance data or schema fields are missing)\n");
     }
      printf("--- End Instance: '%s' ---\n\n", schema->name);
}
