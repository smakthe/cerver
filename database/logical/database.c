#define _GNU_SOURCE // Required for getline on some systems, and strtok_r
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> // For ftruncate, remove, rename, fsync (optional)
#include <pthread.h>
#include <errno.h>  // For perror()
#include <limits.h> // For PATH_MAX
#include "../utils/path_utils.h"
#include "database.h"

// --- Database & Table Creation/Deletion ---

/**
 * @brief Creates a new Database structure.
 * @param name The name for the new database.
 * @return Pointer to the created Database, or NULL on failure.
 */
Database *create_database(const char *name) {
    Database *db = (Database *)malloc(sizeof(Database));
    if (!db) {
        perror("Failed to allocate memory for database");
        return NULL;
    }
    db->name = strdup(name);
    if (!db->name) {
        perror("Failed to duplicate database name");
        free(db);
        return NULL;
    }
    db->table_count = 0;
    // Initialize table pointers to NULL
    for(int i=0; i<MAX_TABLES; ++i) db->tables[i] = NULL;
    printf("Database '%s' created.\n", name);
    return db;
}

/**
 * @brief Creates a new Table within a Database.
 * Initializes the table structure, creates the data file, initializes the B+ Tree index,
 * and sets up the mutex.
 * @param db Pointer to the Database.
 * @param table_name Name for the new table.
 * @param columns Array of strings containing the names of the columns.
 * @param column_count Number of columns.
 * @return Pointer to the created Table, or NULL on failure.
 */
Table *create_table(Database *db, const char *table_name, char **columns, int column_count) {
    // --- Input Validation ---
    if (!db) {
        fprintf(stderr, "Error: Database pointer is NULL in create_table.\n");
        return NULL;
    }
    if (db->table_count >= MAX_TABLES) {
        fprintf(stderr, "Error: Maximum table limit (%d) reached.\n", MAX_TABLES);
        return NULL;
    }
     if (!table_name || strlen(table_name) == 0) {
        fprintf(stderr, "Error: Invalid table name provided.\n");
        return NULL;
    }
    if (!columns || column_count <= 0 || column_count > MAX_COLUMNS) {
         fprintf(stderr, "Error: Invalid column definition (count: %d).\n", column_count);
         return NULL;
    }
     // Check for duplicate table name
     for(int i=0; i<db->table_count; ++i) {
         if(db->tables[i] && strcmp(db->tables[i]->name, table_name) == 0) {
             fprintf(stderr, "Error: Table '%s' already exists in database '%s'.\n", table_name, db->name);
             return NULL;
         }
     }


    // --- Allocation and Initialization ---
    Table *table = (Table *)malloc(sizeof(Table));
    if (!table) {
        perror("Failed to allocate memory for table structure");
        return NULL;
    }
    // Initialize fields to safe values before potential errors
    table->name = NULL;
    table->columns = NULL;
    table->primary_index = NULL;
    table->data_file = NULL;

    table->name = strdup(table_name);
    if (!table->name) {
        perror("Failed to duplicate table name");
        free(table);
        return NULL;
    }

    // Allocate and copy column names provided by the caller
    table->columns = (char **)malloc(column_count * sizeof(char *));
    if (!table->columns) {
        perror("Failed to allocate memory for column names array");
        free(table->name);
        free(table);
        return NULL;
    }
    // Initialize column pointers before strdup loop
    for(int i=0; i<column_count; ++i) table->columns[i] = NULL;

    for (int i = 0; i < column_count; i++) {
        if (!columns[i] || strlen(columns[i]) == 0) {
             fprintf(stderr, "Error: Invalid column name provided at index %d.\n", i);
             // Cleanup previously allocated names
             for (int j = 0; j < i; j++) free(table->columns[j]);
             free(table->columns);
             free(table->name);
             free(table);
             return NULL;
        }
        table->columns[i] = strdup(columns[i]);
        if (!table->columns[i]) {
            perror("Failed to duplicate column name");
            // Cleanup previously allocated names
            for (int j = 0; j < i; j++) free(table->columns[j]);
            free(table->columns);
            free(table->name);
            free(table);
            return NULL;
        }
    }
    table->column_count = column_count;

    // Initialize the B+ Tree index
    table->primary_index = initialize_tree();
    if (!table->primary_index) {
        fprintf(stderr, "Error: Failed to initialize B+ Tree index for table '%s'\n", table_name);
        // Cleanup allocated resources
        for (int i = 0; i < column_count; i++) free(table->columns[i]);
        free(table->columns);
        free(table->name);
        free(table);
        return NULL;
    }

    // Convert table name to lowercase for file creation
    char lowercase_name[FILENAME_BUF_SIZE];
    strncpy(lowercase_name, table_name, FILENAME_BUF_SIZE - 1);
    lowercase_name[FILENAME_BUF_SIZE - 1] = '\0';
    
    for (int i = 0; lowercase_name[i]; i++) {
        lowercase_name[i] = tolower(lowercase_name[i]);
    }

    // Create directory path for the resource if it doesn't exist
    char resource_dir[FILENAME_BUF_SIZE];
    char scaffolded_path[FILENAME_BUF_SIZE];
    
    // Combine project root with scaffolded_resources path
    if (join_project_path(scaffolded_path, sizeof(scaffolded_path), "scaffolded_resources") != 0) {
        fprintf(stderr, "Error creating path to scaffolded_resources\n");
        return NULL;
    }
    
    // Create the full resource directory path
    snprintf(resource_dir, sizeof(resource_dir), "%s/%s", scaffolded_path, lowercase_name);
    
    // Create the directory (mkdir -p equivalent)
    char mkdir_cmd[FILENAME_BUF_SIZE * 2];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", resource_dir);
    system(mkdir_cmd);

    // Construct data filename and open/create the file
    char filename[FILENAME_BUF_SIZE];
    snprintf(filename, sizeof(filename), "%s/%s.dat", resource_dir, lowercase_name);

    // Open in "a+b" (append + binary) first. This creates the file if it doesn't exist.
    FILE* temp_fp = fopen(filename, "a+b");
    if (!temp_fp) {
         perror("Failed to create/open data file initially");
         destroy_tree(table->primary_index); // Cleanup index
         for (int i = 0; i < column_count; i++) free(table->columns[i]);
         free(table->columns);
         free(table->name);
         free(table);
         return NULL;
    }
    fclose(temp_fp); // Close it immediately

    // Now open in "r+b" (read/update binary) mode for actual operations.
    table->data_file = fopen(filename, "r+b");
    if (!table->data_file) {
        perror("Failed to open data file for read/update");
        // Attempt to remove the file we might have just created? Risky.
        // remove(filename);
        destroy_tree(table->primary_index);
        for (int i = 0; i < column_count; i++) free(table->columns[i]);
        free(table->columns);
        free(table->name);
        free(table);
        return NULL;
    }

    // Initialize the mutex for thread safety
    if (pthread_mutex_init(&table->lock, NULL) != 0) {
        perror("Mutex initialization failed");
        fclose(table->data_file);
        destroy_tree(table->primary_index);
        for (int i = 0; i < column_count; i++) free(table->columns[i]);
        free(table->columns);
        free(table->name);
        free(table);
        return NULL;
    }

    // Add the newly created table to the database's list
    db->tables[db->table_count++] = table;
    printf("Table '%s' created successfully in database '%s'.\n", table_name, db->name);
    return table;
}

/**
 * @brief Destroys a Table structure and frees its resources.
 * Closes the data file, destroys the index, frees column names, name, and the struct itself.
 * @param table Pointer to the Table to destroy.
 */
void destroy_table(Table *table) {
     if (!table) return;
     printf("Destroying table '%s'...\n", table->name);

     // Best practice: Lock before destroying, although if called correctly,
     // no other thread should be using it.
     // pthread_mutex_lock(&table->lock); // Optional lock

     // Destroy mutex *after* ensuring no operations are pending
     pthread_mutex_destroy(&table->lock);

     // Close the data file
     if (table->data_file) {
         fclose(table->data_file);
         table->data_file = NULL;
         // Optionally remove the .dat file from disk
         // char filename[FILENAME_BUF_SIZE];
         // snprintf(filename, sizeof(filename), "%s.dat", table->name);
         // remove(filename); // Uncomment to delete file on table destroy
     }

     // Destroy the B+ Tree index
     if (table->primary_index) {
         destroy_tree(table->primary_index);
         table->primary_index = NULL;
     }

     // Free column names
     if (table->columns) {
         for (int i = 0; i < table->column_count; i++) {
             free(table->columns[i]);
         }
         free(table->columns);
         table->columns = NULL;
     }

     // Free table name
     free(table->name);
     table->name = NULL;

     // Free the table structure itself
     free(table);
 }

/**
 * @brief Destroys a Database structure and all its tables.
 * @param db Pointer to the Database to destroy.
 */
void destroy_database(Database *db) {
    if (!db) return;
    printf("Destroying database '%s'...\n", db->name);
    // Iterate through the table pointers and destroy each table
    for (int i = 0; i < db->table_count; i++) {
        if (db->tables[i]) {
            destroy_table(db->tables[i]);
            db->tables[i] = NULL; // Clear the pointer in the array
        }
    }
    // Free the database name
    free(db->name);
    db->name = NULL;
    // Free the database structure itself
    free(db);
}


// --- Row Operations ---

/**
 * @brief Inserts a new row into the table. Appends to file, adds to index.
 * @param table Pointer to the table.
 * @param primary_key The integer primary key for the new row.
 * @param values Array of strings representing the values for each column.
 * @return The file offset of the newly inserted row, or -1 on failure.
 */
long insert_row(Table *table, int primary_key, char **values) {
    if (!table || !table->data_file || !table->primary_index || !values) {
        fprintf(stderr, "Error: Invalid arguments for insert_row.\n");
        return -1;
    }
    long current_offset = -1;
    long result_offset = -1;

    pthread_mutex_lock(&table->lock); // Lock the table for thread safety

    // 1. Check if primary key already exists using the index
    if (search_key(table->primary_index, primary_key) != -1) {
        fprintf(stderr, "Error: Primary key %d already exists in table '%s'. Insertion aborted.\n", primary_key, table->name);
        pthread_mutex_unlock(&table->lock);
        return -1;
    }

    // 2. Seek to the end of the data file to append the new row
    if (fseek(table->data_file, 0, SEEK_END) != 0) {
        perror("Failed to seek to end of file for insert");
        pthread_mutex_unlock(&table->lock);
        return -1;
    }

    // 3. Get the current file offset *before* writing the new row
    // This offset will be stored in the index.
    current_offset = ftell(table->data_file);
    if (current_offset == -1) {
        perror("Failed to get current file offset before insert");
        pthread_mutex_unlock(&table->lock);
        return -1;
    }

    // 4. Write the row data to the file
    // Prepend with a space ' ' (not DELETED_MARKER) to indicate a valid row.
    // Use '|' as delimiter and '\n' at the end.
    if (fprintf(table->data_file, " ") < 1) { // Write the valid marker
         perror("Failed to write row marker");
         pthread_mutex_unlock(&table->lock);
         return -1;
    }
    for (int i = 0; i < table->column_count; i++) {
        // Basic sanitization: replace delimiters within values to prevent parsing errors
        char sanitized_value[MAX_ROW_LEN]; // Use a buffer for sanitization
        strncpy(sanitized_value, values[i] ? values[i] : "", MAX_ROW_LEN - 1);
        sanitized_value[MAX_ROW_LEN - 1] = '\0'; // Ensure null termination
        char *ptr = sanitized_value;
        while (*ptr) {
            if (*ptr == '|' || *ptr == '\n' || *ptr == DELETED_MARKER) {
                *ptr = '_'; // Replace forbidden characters
            }
            ptr++;
        }

        // Write the sanitized value and the appropriate delimiter
        if (fprintf(table->data_file, "%s%c", sanitized_value, (i == table->column_count - 1) ? '\n' : '|') < 0) {
            perror("Failed to write column data");
            // Attempt to truncate back to original position? Very difficult to guarantee consistency.
             pthread_mutex_unlock(&table->lock);
             return -1;
        }
    }

    // 5. Flush the file stream to ensure data is passed to the OS buffer
    if (fflush(table->data_file) != 0) {
        perror("Failed to flush data file after insert");
        // Data might be partially written. Consider the insert failed.
        // We didn't add to index yet, so state is somewhat recoverable.
        pthread_mutex_unlock(&table->lock);
        return -1; // Indicate failure
    }
    // For stronger durability guarantee (at performance cost), use fsync:
    // fsync(fileno(table->data_file));

    // 6. If writing seems successful, insert the primary key and its offset into the B+ Tree index
    insert_key(table->primary_index, primary_key, current_offset);
    result_offset = current_offset; // Set the successful offset to return

    pthread_mutex_unlock(&table->lock); // Unlock the table
    return result_offset;
}

/**
 * @brief Reads a row from the table using the primary key index.
 * @param table Pointer to the table.
 * @param primary_key The primary key of the row to read.
 * @return Newly allocated array of strings (row data). Caller must free. NULL if not found/error.
 */
char **read_row(Table *table, int primary_key) {
    if (!table || !table->data_file || !table->primary_index) return NULL;

    char **row_data = NULL;
    char buffer[MAX_ROW_LEN]; // Buffer to read the row from the file

    pthread_mutex_lock(&table->lock); // Lock for thread safety

    // 1. Search the index for the primary key to get the file offset
    long file_offset = search_key(table->primary_index, primary_key);

    // 2. If key not found in index, the row doesn't exist (or was deleted)
    if (file_offset == -1) {
        // This is a normal "not found" case, not necessarily an error.
        // fprintf(stderr, "Debug: Row with primary key %d not found in index for table '%s'.\n", primary_key, table->name);
        pthread_mutex_unlock(&table->lock);
        return NULL;
    }

    // 3. Seek to the record's position in the data file using the offset
    if (fseek(table->data_file, file_offset, SEEK_SET) != 0) {
        char errorMsg[100];
        snprintf(errorMsg, 100, "Failed to seek to row offset %ld for key %d", file_offset, primary_key);
        perror(errorMsg);
        pthread_mutex_unlock(&table->lock);
        return NULL;
    }

    // 4. Read the line/row from the file into the buffer
    if (fgets(buffer, sizeof(buffer), table->data_file) == NULL) {
        if (feof(table->data_file)) {
             // This indicates a potential inconsistency: index points past EOF.
             fprintf(stderr, "Error: Found offset %ld for key %d, but reached EOF unexpectedly in table '%s'. Possible data corruption.\n", file_offset, primary_key, table->name);
        } else {
             // Other read error
             perror("Failed to read row data from file");
        }
        pthread_mutex_unlock(&table->lock);
        return NULL;
    }

    // 5. Check if the row is marked as deleted (first character)
    if (buffer[0] == DELETED_MARKER) {
        // Row exists physically but is logically deleted. Treat as not found.
        // fprintf(stderr, "Debug: Row with primary key %d found at offset %ld but is marked as deleted.\n", primary_key, file_offset);
        pthread_mutex_unlock(&table->lock);
        return NULL;
    }
     if (buffer[0] != ' ') {
         // First character should be space for valid row or '#' for deleted. Anything else is corruption.
         fprintf(stderr, "Error: Row at offset %ld for key %d has invalid marker character '%c'. Possible data corruption.\n", file_offset, primary_key, buffer[0]);
          pthread_mutex_unlock(&table->lock);
         return NULL;
     }


    // 6. Allocate memory for the result array (char **) to hold column strings
    row_data = (char **)malloc(table->column_count * sizeof(char *));
    if (!row_data) {
        perror("Failed to allocate memory for row data array");
        pthread_mutex_unlock(&table->lock);
        return NULL;
    }
    // Initialize pointers to NULL for easier cleanup on error
    for(int i=0; i < table->column_count; ++i) row_data[i] = NULL;

    // 7. Parse the line using strtok_r (re-entrant version of strtok)
    // Start parsing *after* the initial marker character (space).
    char *line_start = buffer + 1;
    char *token;
    char *saveptr; // Pointer for strtok_r state
    int col_idx = 0;

    token = strtok_r(line_start, "|\n", &saveptr); // Get first token
    while (token != NULL && col_idx < table->column_count) {
        row_data[col_idx] = strdup(token); // Duplicate the token string
        if (!row_data[col_idx]) {
            perror("Failed to duplicate token string");
            // Cleanup already duplicated tokens and the array itself
            for (int j = 0; j < col_idx; j++) free(row_data[j]);
            free(row_data);
            pthread_mutex_unlock(&table->lock);
            return NULL;
        }
        col_idx++;
        token = strtok_r(NULL, "|\n", &saveptr); // Get next token
    }

    // 8. Check if the number of parsed columns matches the expected count
    if (col_idx != table->column_count) {
        fprintf(stderr, "Warning: Row for key %d at offset %ld in table '%s' has %d columns, expected %d. Data might be corrupt or schema mismatch.\n", primary_key, file_offset, table->name, col_idx, table->column_count);
        // Cleanup and return NULL as the data is inconsistent
        for (int j = 0; j < col_idx; j++) free(row_data[j]);
        free(row_data);
        row_data = NULL;
    }

    pthread_mutex_unlock(&table->lock); // Unlock the table
    return row_data; // Return the array of column strings
}

/**
 * @brief Marks a row as deleted in the data file and removes it from the index.
 * @param table Pointer to the table.
 * @param primary_key The primary key of the row to delete.
 * @return 0 on success, -1 on failure.
 */
int delete_row(Table *table, int primary_key) {
    if (!table || !table->data_file || !table->primary_index) return -1;
    int result = -1;

    pthread_mutex_lock(&table->lock); // Lock for thread safety

    // 1. Find the offset of the row using the index
    long file_offset = search_key(table->primary_index, primary_key);

    // 2. If key not found, cannot delete
    if (file_offset == -1) {
        // fprintf(stderr, "Debug: Row with primary key %d not found for deletion in table '%s'.\n", primary_key, table->name);
        pthread_mutex_unlock(&table->lock);
        return -1;
    }

    // 3. Seek to the beginning of the record in the data file
    if (fseek(table->data_file, file_offset, SEEK_SET) != 0) {
        char errorMsg[100];
        snprintf(errorMsg, 100, "Failed to seek to row offset %ld for delete (key %d)", file_offset, primary_key);
        perror(errorMsg);
        pthread_mutex_unlock(&table->lock);
        return -1;
    }

    // 4. Overwrite the first character with the DELETED_MARKER
    if (fputc(DELETED_MARKER, table->data_file) == EOF) {
         perror("Failed to write delete marker");
         pthread_mutex_unlock(&table->lock);
         return -1;
    }

    // 5. Flush the change to ensure the marker is written to OS buffers
     if (fflush(table->data_file) != 0) {
        perror("Failed to flush data file after marking delete");
        // Attempt to undo? fseek back, fputc ' '? Risky. Consider delete failed.
        pthread_mutex_unlock(&table->lock);
        return -1;
    }
    // Optional: fsync for stronger guarantee
    // fsync(fileno(table->data_file));

    // 6. If marking the row seems successful, remove the key from the B+ Tree index
    delete_key(table->primary_index, primary_key);
    result = 0; // Indicate success

    pthread_mutex_unlock(&table->lock); // Unlock the table
    return result;
}

/**
 * @brief Updates a row by marking the old one deleted and inserting the new data.
 * @param table Pointer to the table.
 * @param primary_key The primary key of the row to update (assumed not to change).
 * @param new_values Array of strings with the new column values.
 * @return The new file offset of the updated row, or -1 on failure.
 */
long update_row(Table *table, int primary_key, char **new_values) {
     if (!table || !table->data_file || !table->primary_index || !new_values) return -1;
     long new_offset = -1;

     pthread_mutex_lock(&table->lock); // Lock for the entire update operation

     // --- Step 1: Mark the old row as deleted ---

     // 1a. Find the offset of the existing row
     long old_offset = search_key(table->primary_index, primary_key);
     if (old_offset == -1) {
         fprintf(stderr, "Error: Row with primary key %d not found for update in table '%s'.\n", primary_key, table->name);
         pthread_mutex_unlock(&table->lock);
         return -1;
     }

     // 1b. Seek to the old row's position
     if (fseek(table->data_file, old_offset, SEEK_SET) != 0) {
         char errorMsg[100];
         snprintf(errorMsg, 100, "Failed to seek to old offset %ld for update (key %d)", old_offset, primary_key);
         perror(errorMsg);
         pthread_mutex_unlock(&table->lock);
         return -1;
     }

     // 1c. Write the delete marker
     if (fputc(DELETED_MARKER, table->data_file) == EOF) {
         perror("Failed to write delete marker during update");
         pthread_mutex_unlock(&table->lock);
         return -1;
     }

     // 1d. Flush the marker write
      if (fflush(table->data_file) != 0) {
         perror("Failed to flush delete marker during update");
         pthread_mutex_unlock(&table->lock);
         return -1;
     }
     // Optional: fsync(fileno(table->data_file));


     // --- Step 2: Append the new row data ---

     // 2a. Seek to the end of the file
     if (fseek(table->data_file, 0, SEEK_END) != 0) {
         perror("Failed to seek to end of file for update append");
         // State is inconsistent: old row marked deleted, new not written.
         // Rollback is difficult here without proper transaction log.
         pthread_mutex_unlock(&table->lock);
         return -1;
     }

     // 2b. Get the offset where the new data will start
     new_offset = ftell(table->data_file);
     if (new_offset == -1) {
         perror("Failed to get new offset for update append");
         pthread_mutex_unlock(&table->lock);
         return -1;
     }

     // 2c. Write the new row data (similar to insert_row)
     if (fprintf(table->data_file, " ") < 1) { // Valid marker
         perror("Failed to write row marker for updated row");
          pthread_mutex_unlock(&table->lock);
         return -1;
     }
     for (int i = 0; i < table->column_count; i++) {
         char sanitized_value[MAX_ROW_LEN];
         strncpy(sanitized_value, new_values[i] ? new_values[i] : "", MAX_ROW_LEN - 1);
         sanitized_value[MAX_ROW_LEN - 1] = '\0';
         char *ptr = sanitized_value;
         while (*ptr) {
             if (*ptr == '|' || *ptr == '\n' || *ptr == DELETED_MARKER) *ptr = '_';
             ptr++;
         }
         if (fprintf(table->data_file, "%s%c", sanitized_value, (i == table->column_count - 1) ? '\n' : '|') < 0) {
              perror("Failed to write updated column data");
              pthread_mutex_unlock(&table->lock);
              return -1;
         }
     }

     // 2d. Flush the newly appended data
     if (fflush(table->data_file) != 0) {
         perror("Failed to flush new data during update");
         pthread_mutex_unlock(&table->lock);
         return -1; // New data might be partially written. Inconsistent state.
     }
     // Optional: fsync(fileno(table->data_file));


     // --- Step 3: Update the index ---
     // Remove the old entry (which pointed to old_offset) and insert the new entry
     // pointing to new_offset. Assumes the primary key itself did not change.
     // If PK could change, the logic would need adjustment (delete old PK, insert new PK).
     delete_key(table->primary_index, primary_key);
     insert_key(table->primary_index, primary_key, new_offset);

     pthread_mutex_unlock(&table->lock); // Unlock the table
     return new_offset; // Return the offset of the newly written data
}


// --- Basic Transaction Control (Non-ACID) ---

/**
 * @brief Flushes file buffers to the operating system.
 * Does NOT guarantee data is written to persistent storage (disk).
 * This is NOT an ACID commit.
 * @param table Pointer to the table whose file buffer should be flushed.
 */
void commit_transaction(Table *table) {
    if (!table || !table->data_file) return;
    pthread_mutex_lock(&table->lock);
    if (fflush(table->data_file) != 0) {
        perror("fflush failed during commit_transaction");
    }
    // For stronger durability (at significant performance cost), use fsync:
    // if (fsync(fileno(table->data_file)) != 0) {
    //     perror("fsync failed during commit_transaction");
    // }
    pthread_mutex_unlock(&table->lock);
    // printf("Transaction flushed for table '%s'.\n", table->name); // Optional log
}

/**
 * @brief Clears all data from the table file and re-initializes the index.
 * This is equivalent to TRUNCATE, NOT a rollback of specific operations.
 * @param table Pointer to the table to rollback/clear.
 */
void rollback_transaction(Table *table) {
    if (!table || !table->data_file || !table->primary_index) return;
    pthread_mutex_lock(&table->lock);

    // 1. Truncate the physical data file to zero length
    if (ftruncate(fileno(table->data_file), 0) != 0) {
         perror("ftruncate failed during rollback_transaction");
         // Continue to clear index anyway, but file state is uncertain.
    }
    // Reset file pointer to the beginning after truncation
    rewind(table->data_file);

    // 2. Destroy the existing B+ Tree index
    destroy_tree(table->primary_index);

    // 3. Initialize a new, empty B+ Tree index
    table->primary_index = initialize_tree();
    if (!table->primary_index) {
        // This is a critical error, the table is now unusable.
        fprintf(stderr, "CRITICAL ERROR: Failed to re-initialize index during rollback for table '%s'. Table is inconsistent.\n", table->name);
        // Consider exiting or marking the table as unusable.
    }

    pthread_mutex_unlock(&table->lock);
    printf("Transaction rolled back (table '%s' cleared).\n", table->name);
}

// --- Utility Functions ---

/**
 * @brief Prints the schema information (tables and columns) for the database.
 * @param db Pointer to the Database.
 */
void print_database(Database *db) {
    if (!db) {
        printf("Cannot print NULL database.\n");
        return;
    }
    printf("\n--- Database Schema: '%s' ---\n", db->name);
    printf("Tables (%d):\n", db->table_count);
    if (db->table_count == 0) {
        printf("  (No tables defined)\n");
    } else {
        for (int i = 0; i < db->table_count; i++) {
            Table *t = db->tables[i];
            if (t) {
                printf("  - Table: '%s' (Columns: %d)\n", t->name, t->column_count);
                printf("      Columns: ");
                 for(int j=0; j<t->column_count; ++j) {
                     printf("%s%s", t->columns[j] ? t->columns[j] : "??", (j == t->column_count - 1) ? "" : ", ");
                 }
                 printf("\n");
            } else {
                 printf("  - Table [%d]: NULL pointer\n", i);
            }
        }
    }
     printf("--- End Schema ---\n\n");
}

/**
 * @brief Compacts the table's data file by removing deleted rows.
 * Reads the old file, writes valid rows to a temporary file, rebuilds the index,
 * and replaces the old file with the temporary file. This is an I/O intensive operation.
 * @param table Pointer to the Table to compact.
 */
void compact_table(Table *table) {
    if (!table || !table->data_file || !table->primary_index) {
         fprintf(stderr, "Error: Cannot compact invalid table.\n");
         return;
    }

    pthread_mutex_lock(&table->lock); // Lock the table during compaction
    printf("Compacting table '%s'...\n", table->name);

    // --- Setup: Temp file and new index ---
    char temp_filename[FILENAME_BUF_SIZE];
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", table->name);

    // Open temporary file for writing binary data
    FILE *temp_file = fopen(temp_filename, "wb");
    if (!temp_file) {
        perror("Failed to open temporary file for compaction");
        pthread_mutex_unlock(&table->lock);
        return;
    }

    // Create a new B+ Tree index to be populated during compaction
    BPlusTree *new_index = initialize_tree();
    if (!new_index) {
         fprintf(stderr, "Error: Failed to create new index for compaction\n");
         fclose(temp_file);
         remove(temp_filename); // Clean up temp file
         pthread_mutex_unlock(&table->lock);
         return;
    }

    // --- Process old file: Read, Filter, Write, Re-index ---
    rewind(table->data_file); // Start reading from the beginning of the old file
    char buffer[MAX_ROW_LEN];
    long current_write_offset = 0; // Tracks offset in the *new* temp file

    while (fgets(buffer, sizeof(buffer), table->data_file) != NULL) {
        // Check if the row is valid (not marked as deleted)
        if (buffer[0] != DELETED_MARKER && buffer[0] == ' ') {
            // This is a valid row, write it to the temp file
            // Get primary key from the row data (assuming first column is PK)
             char temp_buffer_for_parse[MAX_ROW_LEN]; // Use copy for strtok
             strncpy(temp_buffer_for_parse, buffer + 1, MAX_ROW_LEN -1); // Copy data part
             temp_buffer_for_parse[MAX_ROW_LEN -1] = '\0';

             char *first_col = strtok(temp_buffer_for_parse, "|"); // Modifies temp_buffer_for_parse
             if (first_col) {
                 int primary_key = atoi(first_col); // Convert PK string to int

                 // Write the original valid row (including the space marker) to the temp file
                 if (fputs(buffer, temp_file) == EOF) {
                      perror("Failed to write row to temp file during compaction");
                      fclose(temp_file); remove(temp_filename); destroy_tree(new_index);
                      pthread_mutex_unlock(&table->lock); return;
                 }

                 // Add the key and its *new* offset in the temp file to the new index
                 insert_key(new_index, primary_key, current_write_offset);

                 // Update the write offset for the next valid row
                 // ftell after fputs gives the position *after* the write
                 current_write_offset = ftell(temp_file);
                 if(current_write_offset == -1) {
                     perror("ftell failed on temp file during compaction");
                     fclose(temp_file); remove(temp_filename); destroy_tree(new_index);
                     pthread_mutex_unlock(&table->lock); return;
                 }
             } else {
                 // Should not happen with valid data, indicates potential corruption
                 fprintf(stderr, "Warning: Could not parse primary key during compaction for row: %s", buffer);
             }
        }
        // If row starts with DELETED_MARKER, skip it (don't write to temp file)
    }

    // --- Finalization: Replace files and index ---
    // Ensure all data is written to the temp file buffer
    if (fflush(temp_file) != 0) {
         perror("Failed to flush temp file before closing");
         fclose(temp_file); remove(temp_filename); destroy_tree(new_index);
         pthread_mutex_unlock(&table->lock); return;
    }
    // Optional: fsync(fileno(temp_file));
    fclose(temp_file); // Close the temporary file

    // Close the old data file (it's no longer needed)
    fclose(table->data_file);
    table->data_file = NULL; // Mark as closed

    // Replace the old data file with the compacted temporary file
    char old_filename[FILENAME_BUF_SIZE];
    snprintf(old_filename, sizeof(old_filename), "%s.dat", table->name);
    if (remove(old_filename) != 0) {
        // If remove fails, the original file might still exist.
        perror("Failed to remove old data file during compaction");
        remove(temp_filename); // Try to clean up temp file
        destroy_tree(new_index); // Free the index we built
        // Attempt to reopen old file? Table state is inconsistent.
        fprintf(stderr, "Error: Compaction failed for table '%s'. Original file may still exist, but index is lost.\n", table->name);
        pthread_mutex_unlock(&table->lock);
        return;
    }
    if (rename(temp_filename, old_filename) != 0) {
         // If rename fails, the old file is gone, but the temp file couldn't be renamed.
         perror("Failed to rename temporary data file during compaction");
         destroy_tree(new_index);
         fprintf(stderr, "Error: Compaction failed for table '%s'. Data file lost or inaccessible.\n", table->name);
         pthread_mutex_unlock(&table->lock);
         return;
    }

    // Reopen the newly compacted data file for the table
    table->data_file = fopen(old_filename, "r+b");
    if (!table->data_file) {
        perror("Failed to reopen compacted data file");
        destroy_tree(new_index); // Free the new index as we can't use it
        fprintf(stderr, "Error: Compaction failed for table '%s'. Could not reopen data file.\n", table->name);
        pthread_mutex_unlock(&table->lock);
        return;
    }

    // Replace the old index with the newly built one
    destroy_tree(table->primary_index); // Free the old index
    table->primary_index = new_index;   // Assign the new index

    printf("Table '%s' compacted successfully.\n", table->name);
    pthread_mutex_unlock(&table->lock); // Release the lock
}
