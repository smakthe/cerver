#ifndef TYPE_MAP_H
#define TYPE_MAP_H

// Returns the C type string for a given scaffold type name.
// e.g. "string" -> "char", "int" -> "int", "boolean" -> "int"
const char* map_to_c_type(const char *scaffold_type);

// Returns the C declaration string for a named field.
// For array types (string, text, date) this includes the size.
// e.g. map_to_c_declaration("title", "string") -> "char title[256]"
// e.g. map_to_c_declaration("price", "float")  -> "float price"
// Writes into caller-provided buffer. Returns buffer pointer.
char* map_to_c_declaration(const char *field_name, const char *scaffold_type,
                           char *buffer, int buf_size);

// Returns 1 if the scaffold type is stored as a fixed-size char array in C.
// (i.e. string, text, date). Returns 0 for scalar types (int, float, etc.)
int is_array_type(const char *scaffold_type);

// Returns the array size for array types, 0 for scalar types.
int get_array_size(const char *scaffold_type);

#endif // TYPE_MAP_H