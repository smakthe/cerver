#ifndef DATABASE_H
#define DATABASE_H

#include "../physical/b_plus_tree.h" // Include B+ Tree definitions
#include <pthread.h>                 // For thread safety (mutex)
#include <stdio.h>                   // For FILE type

// --- Configuration Constants ---
#define MAX_TABLES 100      // Maximum number of tables allowed in a database
#define MAX_COLUMNS 100     // Maximum number of columns allowed in a table
#define MAX_ROW_LEN 4096    // Maximum expected length of a single row string (for buffer safety)
#define FILENAME_BUF_SIZE 256 // Buffer size for constructing filenames
#define DELETED_MARKER '#'  // Character used to mark rows as deleted in the data file

// --- Structures ---

// Represents a table within the database
typedef struct Table {
    char *name;                 // Name of the table
    char **columns;             // Array of column name strings
    int column_count;           // Number of columns in the table
    BPlusTree *primary_index;   // B+ Tree index on the primary key
    FILE *data_file;            // File pointer to the data file (.dat) storing rows
    pthread_mutex_t lock;       // Mutex to ensure thread-safe access to the table
} Table;

// Represents the database itself
typedef struct Database {
    char *name;                 // Name of the database
    Table *tables[MAX_TABLES];  // Array of pointers to tables within the database
    int table_count;            // Current number of tables in the database
} Database;

// --- Function Prototypes ---

// Database Management
Database *create_database(const char *name); // Creates a new database structure
// Creates a new table within the database.
// Columns is an array of strings representing column names.
Table *create_table(Database *db, const char *table_name, char **columns, int column_count);
void destroy_database(Database *db); // Frees all resources associated with the database and its tables

// Row Operations (Primary Key is assumed to be the first column and an integer)

/**
 * Inserts a new row into the table.
 * @param table Pointer to the table.
 * @param primary_key The integer primary key for the new row.
 * @param values Array of strings representing the values for each column.
 * @return The file offset of the newly inserted row, or -1 on failure (e.g., PK exists, IO error).
 */
long insert_row(Table *table, int primary_key, char **values);

/**
 * Reads a row from the table based on its primary key.
 * @param table Pointer to the table.
 * @param primary_key The primary key of the row to read.
 * @return A newly allocated array of strings containing the row data.
 * The caller is responsible for freeing each string in the array
 * and the array itself using free(). Returns NULL if the row is
 * not found, marked as deleted, or an error occurs.
 */
char **read_row(Table *table, int primary_key);

/**
 * Updates an existing row in the table.
 * This currently marks the old row as deleted and appends the new row data.
 * @param table Pointer to the table.
 * @param primary_key The primary key of the row to update.
 * @param new_values Array of strings representing the new values for the row.
 * @return The new file offset of the updated row data, or -1 on failure (e.g., row not found, IO error).
 */
long update_row(Table *table, int primary_key, char **new_values);

/**
 * Deletes a row from the table based on its primary key.
 * This currently marks the row as deleted in the data file and removes it from the index.
 * It does not immediately reclaim file space (requires compaction).
 * @param table Pointer to the table.
 * @param primary_key The primary key of the row to delete.
 * @return 0 on success, -1 on failure (e.g., row not found, IO error).
 */
int delete_row(Table *table, int primary_key);

// Basic Transaction Control (Limitations Apply)
// NOTE: These are NOT ACID-compliant transactions.
void commit_transaction(Table *table); // Flushes file buffers to the OS (does not guarantee disk write)
void rollback_transaction(Table *table); // Clears all data from the table file and index (effectively TRUNCATE)

// Utility Functions
void print_database(Database *db); // Prints the names of tables and columns in the database
void compact_table(Table *table);  // Rewrites the data file to remove deleted rows and rebuilds the index. Expensive operation.

#endif // DATABASE_H
