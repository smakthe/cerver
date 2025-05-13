#ifndef ORM_H
#define ORM_H

#include "../logical/database.h" // Include logical layer definitions

// --- Structures for ORM Schema Definition ---

/**
 * @brief Defines the properties of a field (column) in a model's schema.
 */
typedef struct Field {
    char *name;                 // Name of the field (corresponds to column name)
    char *type;                 // Data type hint (e.g., "int", "string", "text") - used for validation/ORM logic
    int is_primary;             // Flag: 1 if this field is the primary key, 0 otherwise
    int is_foreign_key;         // Flag: 1 if this field is a foreign key, 0 otherwise
    char *referenced_table;     // Name of the table referenced by the foreign key (if applicable)
    char *referenced_column;    // Name of the column referenced by the foreign key (if applicable)
    // Example validation function pointer (can be extended):
    // int (*validate)(struct Field *field, const char *value); // Return 0 if valid
} Field;

/**
 * @brief Defines a relationship (association) between models in the schema.
 * (Currently conceptual, not fully implemented in operations).
 */
typedef struct Association {
    char *type;                 // Type of association (e.g., "belongs_to", "has_many")
    char *related_model;        // Name of the related model
    char *foreign_key;          // Name of the foreign key field involved in the relationship
                                // (in this model for belongs_to, in the other for has_many)
} Association;

/**
 * @brief Defines the schema (structure) of a data model, mapping to a database table.
 */
typedef struct Model {
    char *name;                 // Name of the model (usually corresponds to table name)
    Table *table_ref;           // Pointer to the underlying logical Table structure
    Field *fields;              // Array defining the fields (columns) of this model
    int field_count;            // Number of fields in the model
    Association *associations;  // Array defining relationships with other models (conceptual)
    int association_count;      // Number of associations
    // --- Callback Function Pointers (Example Hooks) ---
    // Operate on ModelInstance data before/after persistence operations.
    // void (*before_save)(struct ModelInstance *instance);
    // void (*after_save)(struct ModelInstance *instance);
    // void (*before_delete)(struct ModelInstance *instance);
    // void (*after_delete)(struct ModelInstance *instance);
    // int (*validate)(struct ModelInstance *instance); // Returns 0 if valid
} Model;


// --- Structure for ORM Data Instance ---

/**
 * @brief Represents an actual instance (row) of a data model, holding the data.
 */
typedef struct ModelInstance {
    Model *model_schema;    // Pointer back to the Model schema definition
    char **data;            // Array of strings holding the actual data for each field.
                            // data[i] corresponds to model_schema->fields[i].
                            // Data is stored as strings, matching the logical layer.
    long record_offset;     // The file offset of this record in the data file.
                            // Set to -1 for new instances not yet saved or after deletion.
} ModelInstance;


// --- Global Database Access ---
// Provides a single point of access to the database for the ORM.
// Consider dependency injection or context passing for more complex applications.
extern Database *global_db;

// --- Function Prototypes ---

// Database Initialization (called once by API layer)
Database *initialize_database(const char *name); // Initializes the global_db pointer

// Model Schema Management
// Defines a model schema and creates the corresponding logical table.
// Assumes 'fields' and 'associations' arrays persist for the lifetime of the Model.
Model *define_model(const char *name, Field *fields, int field_count, Association *associations, int association_count);
void destroy_model_schema(Model* model); // Frees the Model struct (not the underlying table or fields/associations memory)

// Model Instance Management
ModelInstance* create_new_instance(Model* model_schema); // Allocates an empty instance linked to a schema
void free_model_instance(ModelInstance* instance); // Frees an instance and its data array/strings
// Sets a field in a ModelInstance by index, handling string duplication. (Moved from main.c)
int set_instance_field(ModelInstance *instance, int field_index, const char *value);


// ORM CRUD Operations (Operating on Model Instances)

/**
 * Saves a model instance to the database.
 * Performs an INSERT if the instance is new (record_offset == -1).
 * Performs an UPDATE if the instance exists (record_offset != -1).
 * Updates instance->record_offset on successful insert/update.
 * @param instance Pointer to the ModelInstance to save.
 * @return 0 on success, -1 on failure (validation, DB error, PK violation).
 */
int save_model_instance(ModelInstance *instance);

/**
 * Deletes a model instance from the database.
 * Marks the row as deleted in the file and removes it from the index.
 * Sets instance->record_offset to -1 on success.
 * @param instance Pointer to the ModelInstance to delete. Must have a valid record_offset.
 * @return 0 on success, -1 on failure (not found, DB error).
 */
int delete_model_instance(ModelInstance *instance);

/**
 * Finds a model instance by its primary key.
 * @param model_schema Pointer to the schema of the model to find.
 * @param primary_key The integer primary key value to search for.
 * @return A pointer to a *newly allocated* ModelInstance containing the found data.
 * The caller is responsible for freeing this instance using free_model_instance().
 * Returns NULL if the record is not found, deleted, or an error occurs.
 */
ModelInstance* find_model_by_primary_key(Model *model_schema, int primary_key);

// Utility / Schema Inspection / Configuration
// Adds foreign key metadata to a field in the model schema (for informational purposes).
void add_foreign_key(Model *model, const char *field_name, const char *referenced_table, const char *referenced_column);
// Prints the schema definition of a model.
void print_model_schema(Model *model);
// Prints the data contained within a model instance.
void print_model_instance(ModelInstance *instance);

// Example for setting callbacks (adapt if implementing callback logic)
// void set_callbacks(Model *model, void (*before_save)(ModelInstance*), ...);

#endif // ORM_H
