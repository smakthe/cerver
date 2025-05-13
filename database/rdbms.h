#ifndef RDBMS_H
#define RDBMS_H

#include "application/orm.h" // Include ORM types (Model, ModelInstance, Field, etc.)

// --- System Initialization and Shutdown ---

/**
 * @brief Initializes the database system and the global database connection.
 * Must be called once before any other database operations.
 * @param db_name The name for the database (used for potential file naming).
 * @return 0 on success, -1 on failure.
 */
int db_system_init(const char* db_name);

/**
 * @brief Shuts down the database system cleanly.
 * Destroys the database, frees associated resources (tables, indexes),
 * and cleans up the model registry. Should be called on application exit.
 */
void db_system_shutdown();


// --- Model Definition ---

/**
 * @brief Defines a data model schema and registers it with the API.
 * This creates the underlying database table if it doesn't exist.
 * Wrapper around the ORM's define_model.
 * @param name The name of the model (e.g., "User", "Post").
 * @param fields An array of Field structs defining the model's columns.
 * @param field_count The number of fields in the array.
 * @param associations An array of Association structs (currently conceptual).
 * @param association_count The number of associations.
 * @return Pointer to the created Model schema structure (managed internally, do not free directly),
 * or NULL on failure.
 */
Model* db_define_model(const char *name, Field *fields, int field_count, Association *associations, int association_count);


// --- Instance Management & Data Access ---

/**
 * @brief Creates a new, empty instance of a specified model.
 * The instance is not saved to the database until db_save() is called.
 * @param model_name The name of the model schema to instantiate (must have been defined via db_define_model).
 * @return Pointer to a newly allocated ModelInstance, or NULL if the model name is not found or allocation fails.
 * The caller is responsible for freeing this instance using db_free_instance().
 */
ModelInstance* db_create_instance(const char* model_name);

/**
 * @brief Sets the value of a field within a ModelInstance.
 * Handles memory management for the string value (duplicates the input string).
 * @param instance Pointer to the ModelInstance to modify.
 * @param field_name The name of the field to set.
 * @param value The string value to set for the field. Passing NULL clears the field.
 * @return 0 on success, -1 on failure (e.g., invalid instance, field not found, memory error).
 */
int db_set_field(ModelInstance* instance, const char* field_name, const char* value);

/**
 * @brief Gets the string value of a field from a ModelInstance.
 * @param instance Pointer to the ModelInstance.
 * @param field_name The name of the field to retrieve.
 * @return Pointer to the string value within the instance's data array,
 * or NULL if the instance is invalid, the field doesn't exist, or the field's value is NULL.
 * Do NOT free the returned pointer; it points to internal instance data.
 */
const char* db_get_field(ModelInstance* instance, const char* field_name);

/**
 * @brief Frees the memory associated with a ModelInstance, including its data array and strings.
 * @param instance Pointer to the ModelInstance to free.
 */
void db_free_instance(ModelInstance* instance);


// --- CRUD Operations ---

/**
 * @brief Saves a ModelInstance to the database (performs INSERT or UPDATE).
 * Wrapper around the ORM's save_model_instance.
 * @param instance Pointer to the ModelInstance to save.
 * @return 0 on success, -1 on failure.
 */
int db_save(ModelInstance* instance);

/**
 * @brief Finds a ModelInstance in the database by its primary key.
 * Wrapper around the ORM's find_model_by_primary_key.
 * @param model_name The name of the model to search within.
 * @param primary_key The integer primary key value to find.
 * @return Pointer to a *newly allocated* ModelInstance if found, or NULL otherwise.
 * The caller is responsible for freeing the returned instance using db_free_instance().
 */
ModelInstance* db_find_by_pk(const char* model_name, int primary_key);

/**
 * @brief Deletes a ModelInstance from the database.
 * Wrapper around the ORM's delete_model_instance.
 * @param instance Pointer to the ModelInstance to delete. Must have been previously saved (have a valid record_offset).
 * @return 0 on success, -1 on failure.
 */
int db_delete(ModelInstance* instance);


// --- Utility (Optional) ---
// You might add other API functions here, e.g., for querying based on non-PK fields (would require significant extension),
// or for triggering table compaction.

/**
 * @brief Triggers compaction for a specific table.
 * Rewrites the table's data file to remove deleted rows. Expensive operation.
 * @param model_name The name of the model whose table should be compacted.
 * @return 0 on success, -1 on failure (e.g., model not found).
 */
int db_compact_table(const char* model_name);


#endif // RDBMS_H
