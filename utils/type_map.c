#include <stdio.h>
#include <string.h>
#include "type_map.h"

typedef struct {
    const char *scaffold_type;
    const char *c_type;
    int         array_size;   // 0 means scalar
} TypeEntry;

static const TypeEntry type_table[] = {
    { "int",     "int",   0    },
    { "float",   "float", 0    },
    { "double",  "double",0    },
    { "char",    "char",  0    },
    { "boolean", "int",   0    },
    { "bool",    "int",   0    },
    { "string",  "char",  256  },
    { "text",    "char",  1024 },
    { "date",    "char",  11   },  /* "YYYY-MM-DD\0" */
    { NULL, NULL, 0 }
};

static const TypeEntry* find_entry(const char *scaffold_type) {
    if (!scaffold_type) return NULL;
    for (int i = 0; type_table[i].scaffold_type != NULL; i++) {
        if (strcmp(type_table[i].scaffold_type, scaffold_type) == 0) {
            return &type_table[i];
        }
    }
    return NULL;
}

const char* map_to_c_type(const char *scaffold_type) {
    const TypeEntry *e = find_entry(scaffold_type);
    if (!e) return "char"; // safe fallback — treat unknowns as char array
    return e->c_type;
}

int is_array_type(const char *scaffold_type) {
    const TypeEntry *e = find_entry(scaffold_type);
    if (!e) return 1; // unknown types fall back to array
    return e->array_size > 0;
}

int get_array_size(const char *scaffold_type) {
    const TypeEntry *e = find_entry(scaffold_type);
    if (!e) return 256; // unknown types fall back to string size
    return e->array_size;
}

char* map_to_c_declaration(const char *field_name, const char *scaffold_type,
                           char *buffer, int buf_size) {
    const TypeEntry *e = find_entry(scaffold_type);

    if (!e) {
        // Unknown type — emit as char[256] with a comment
        snprintf(buffer, buf_size, "char %s[256] /* unknown type: %s */",
                 field_name, scaffold_type);
        return buffer;
    }

    if (e->array_size > 0) {
        snprintf(buffer, buf_size, "%s %s[%d]",
                 e->c_type, field_name, e->array_size);
    } else {
        snprintf(buffer, buf_size, "%s %s",
                 e->c_type, field_name);
    }

    return buffer;
}