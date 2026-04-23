#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "scaffold_controller.h"
#include "../utils/path_utils.h"
#include "../models/scaffold_model.h"
#include "../models/model_setup.h"
#include "../database/application/orm.h"
#include "../database/physical/b_plus_tree.h"

#define MAX_MODEL_NAME 100
#define MAX_JSON_SIZE 4096
#define MAX_INDEX_JSON_SIZE 65536   /* 64 KB for list responses */

// Create a success result
ControllerResult* create_success_result(const char *message, void *data, int data_size) {
    ControllerResult *result = malloc(sizeof(ControllerResult));
    if (!result) return NULL;
    result->success = 1;
    result->message = message ? strdup(message) : NULL;
    result->data = data;
    result->data_size = data_size;
    return result;
}

// Create an error result
ControllerResult* create_error_result(const char *message) {
    ControllerResult *result = malloc(sizeof(ControllerResult));
    if (!result) return NULL;
    result->success = 0;
    result->message = message ? strdup(message) : NULL;
    result->data = NULL;
    result->data_size = 0;
    return result;
}

// Free a controller result
void free_controller_result(ControllerResult *result) {
    if (!result) return;
    if (result->message) free(result->message);
    free(result);
}

// Utility: Trim whitespace from a string
char* trim_whitespace(char *str) {
    if (!str) return NULL;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    *(end+1) = '\0';
    return str;
}

// Utility: Parse a JSON field from a JSON string
char* parse_json_field(const char *json, const char *field_name) {
    if (!json || !field_name) return NULL;

    char field_search[256];
    snprintf(field_search, sizeof(field_search), "\"%s\":", field_name);

    const char *field_pos = strstr(json, field_search);
    if (!field_pos) return NULL;

    field_pos += strlen(field_search);
    while (isspace(*field_pos)) field_pos++;

    if (*field_pos == '"') {
        field_pos++;
        const char *end_quote = strchr(field_pos, '"');
        if (!end_quote) return NULL;
        size_t value_len = end_quote - field_pos;
        char *value = malloc(value_len + 1);
        if (!value) return NULL;
        strncpy(value, field_pos, value_len);
        value[value_len] = '\0';
        return value;
    } else if (isdigit(*field_pos) || *field_pos == '-' ||
               strncmp(field_pos, "true", 4) == 0 ||
               strncmp(field_pos, "false", 5) == 0 ||
               strncmp(field_pos, "null", 4) == 0) {
        const char *end_value = field_pos;
        while (*end_value && *end_value != ',' && *end_value != '}' && *end_value != ']') {
            end_value++;
        }
        size_t value_len = end_value - field_pos;
        char *value = malloc(value_len + 1);
        if (!value) return NULL;
        strncpy(value, field_pos, value_len);
        value[value_len] = '\0';
        return trim_whitespace(value);
    }

    return NULL;
}

// Generate a JSON response
char* generate_json_response(int success, const char *message, const char *data) {
    char *json = malloc(MAX_JSON_SIZE);
    if (!json) return NULL;
    strcpy(json, "{");
    sprintf(json + strlen(json), "\"status\": \"%s\"", success ? "success" : "error");
    if (message) sprintf(json + strlen(json), ", \"message\": \"%s\"", message);
    if (data)    sprintf(json + strlen(json), ", \"data\": %s", data);
    strcat(json, "}");
    return json;
}

// Internal helper: serialise a ModelInstance to a JSON object string.
// Returns a malloc'd string — caller must free.
static char* build_instance_json(Model *schema, ModelInstance *instance) {
    char *json = malloc(MAX_JSON_SIZE);
    if (!json) return NULL;
    strcpy(json, "{");
    for (int i = 0; i < schema->field_count; i++) {
        char field_buf[512];
        snprintf(field_buf, sizeof(field_buf), "\"%s\": \"%s\"",
                 schema->fields[i].name,
                 instance->data[i] ? instance->data[i] : "");
        strcat(json, field_buf);
        if (i < schema->field_count - 1) strcat(json, ", ");
    }
    strcat(json, "}");
    return json;
}

// Controller function to list all resources (index action)
ControllerResult* indx(const char *model_name) {
    printf("Listing all %s resources...\n", model_name);

    Model *schema = find_model_by_name(model_name);
    if (!schema || !schema->table_ref) {
        return create_error_result("Model not found or not initialised");
    }

    // Collect all primary keys from the B+ tree leaf list
    int count = 0;
    int *keys = collect_all_keys(schema->table_ref->primary_index, &count);

    char *json = malloc(MAX_INDEX_JSON_SIZE);
    if (!json) {
        if (keys) free(keys);
        return create_error_result("Memory allocation failed");
    }
    strcpy(json, "[");

    for (int i = 0; i < count; i++) {
        ModelInstance *instance = find_model_by_primary_key(schema, keys[i]);
        if (!instance) continue;

        char *obj = build_instance_json(schema, instance);
        free_model_instance(instance);

        if (obj) {
            if (i > 0) strcat(json, ", ");
            // Guard against overflow
            if (strlen(json) + strlen(obj) + 4 < MAX_INDEX_JSON_SIZE) {
                strcat(json, obj);
            }
            free(obj);
        }
    }

    strcat(json, "]");
    if (keys) free(keys);

    return create_success_result("Resources retrieved successfully", json, strlen(json));
}

// Controller function to view a single resource (view action)
ControllerResult* view(const char *model_name, int id) {
    printf("Viewing %s with ID %d...\n", model_name, id);

    if (id <= 0) return create_error_result("Invalid resource ID");

    Model *schema = find_model_by_name(model_name);
    if (!schema) return create_error_result("Model not found");

    ModelInstance *instance = find_model_by_primary_key(schema, id);
    if (!instance) return create_error_result("Resource not found");

    char *json = build_instance_json(schema, instance);
    free_model_instance(instance);

    if (!json) return create_error_result("Failed to serialise resource");
    return create_success_result("Resource retrieved successfully", json, strlen(json));
}

// Controller function to create a new resource (create action)
ControllerResult* create(const char *model_name, char *data) {
    printf("Creating new %s...\n", model_name);

    if (!data || data[0] != '{') return create_error_result("Invalid JSON data");

    Model *schema = find_model_by_name(model_name);
    if (!schema) return create_error_result("Model not found");

    ModelInstance *instance = create_new_instance(schema);
    if (!instance) return create_error_result("Failed to allocate instance");

    // Parse every field defined in the schema from the incoming JSON
    for (int i = 0; i < schema->field_count; i++) {
        char *value = parse_json_field(data, schema->fields[i].name);
        if (value) {
            set_instance_field(instance, i, value);
            free(value);
        }
    }

    if (save_model_instance(instance) != 0) {
        free_model_instance(instance);
        return create_error_result("Failed to save resource");
    }

    char *json = build_instance_json(schema, instance);
    free_model_instance(instance);

    if (!json) return create_error_result("Failed to serialise resource");
    return create_success_result("Resource created successfully", json, strlen(json));
}

// Controller function to update an existing resource (update action)
ControllerResult* update(const char *model_name, int id, char *data) {
    printf("Updating %s with ID %d...\n", model_name, id);

    if (id <= 0) return create_error_result("Invalid resource ID");
    if (!data || data[0] != '{') return create_error_result("Invalid JSON data");

    Model *schema = find_model_by_name(model_name);
    if (!schema) return create_error_result("Model not found");

    ModelInstance *instance = find_model_by_primary_key(schema, id);
    if (!instance) return create_error_result("Resource not found");

    // Update every non-PK field present in the JSON body
    for (int i = 1; i < schema->field_count; i++) {
        char *value = parse_json_field(data, schema->fields[i].name);
        if (value) {
            set_instance_field(instance, i, value);
            free(value);
        }
    }

    if (save_model_instance(instance) != 0) {
        free_model_instance(instance);
        return create_error_result("Failed to update resource");
    }

    char *json = build_instance_json(schema, instance);
    free_model_instance(instance);

    if (!json) return create_error_result("Failed to serialise resource");
    return create_success_result("Resource updated successfully", json, strlen(json));
}

// Controller function to fully replace a resource (PUT action)
ControllerResult* replace(const char *model_name, int id, char *data) {
    printf("Replacing %s with ID %d...\n", model_name, id);

    if (id <= 0) return create_error_result("Invalid resource ID");
    if (!data || data[0] != '{') return create_error_result("Invalid JSON data");

    Model *schema = find_model_by_name(model_name);
    if (!schema) return create_error_result("Model not found");

    ModelInstance *instance = find_model_by_primary_key(schema, id);
    if (!instance) return create_error_result("Resource not found");

    // Overwrite ALL fields (including non-PK) from the payload.
    // Fields absent from the payload are cleared to empty string.
    for (int i = 1; i < schema->field_count; i++) {
        char *value = parse_json_field(data, schema->fields[i].name);
        if (value) {
            set_instance_field(instance, i, value);
            free(value);
        } else {
            // PUT semantics: missing field means clear it
            set_instance_field(instance, i, "");
        }
    }

    if (save_model_instance(instance) != 0) {
        free_model_instance(instance);
        return create_error_result("Failed to replace resource");
    }

    char *json = build_instance_json(schema, instance);
    free_model_instance(instance);

    if (!json) return create_error_result("Failed to serialise resource");
    return create_success_result("Resource replaced successfully", json, strlen(json));
}

// Controller function to delete a resource (destroy action)
ControllerResult* destroy(const char *model_name, int id) {
    printf("Deleting %s with ID %d...\n", model_name, id);

    if (id <= 0) return create_error_result("Invalid resource ID");

    Model *schema = find_model_by_name(model_name);
    if (!schema) return create_error_result("Model not found");

    ModelInstance *instance = find_model_by_primary_key(schema, id);
    if (!instance) return create_error_result("Resource not found");

    int result = delete_model_instance(instance);
    free_model_instance(instance);

    if (result != 0) return create_error_result("Failed to delete resource");
    return create_success_result("Resource deleted successfully", NULL, 0);
}

// Convert string to lowercase
char* controller_to_lowercase(const char* str) {
    if (!str) return NULL;
    char* result = strdup(str);
    for (int i = 0; result[i]; i++) result[i] = tolower(result[i]);
    return result;
}

void generate_controller_code(const char *model_name) {
    char* lowercase_name = controller_to_lowercase(model_name);

    char resource_dir[PATH_MAX];
    char scaffolded_path[PATH_MAX];

    if (join_project_path(scaffolded_path, sizeof(scaffolded_path), "scaffolded_resources") != 0) {
        fprintf(stderr, "Error creating path to scaffolded_resources\n");
        free(lowercase_name);
        return;
    }

    snprintf(resource_dir, sizeof(resource_dir), "%s/%s", scaffolded_path, lowercase_name);

    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", resource_dir);
    system(mkdir_cmd);

    FILE *controller_file;
    char controller_filename[512];
    snprintf(controller_filename, sizeof(controller_filename),
             "%s/%s_controller.c", resource_dir, lowercase_name);

    controller_file = fopen(controller_filename, "w");
    if (controller_file == NULL) {
        printf("Error opening file for controller generation\n");
        free(lowercase_name);
        return;
    }

    printf("Creating controller file: %s\n", controller_filename);

    // Includes
    fprintf(controller_file, "#include <stdio.h>\n");
    fprintf(controller_file, "#include <stdlib.h>\n");
    fprintf(controller_file, "#include <string.h>\n");
    fprintf(controller_file, "#include <ctype.h>\n");
    fprintf(controller_file, "#include \"../../../controllers/scaffold_controller.h\"\n");
    fprintf(controller_file, "#include \"%s.h\"\n\n", lowercase_name);

    // ------------------------------------------------------------------
    // Each generated function is a thin delegation wrapper around the
    // generic runtime functions in scaffold_controller.c, which handle
    // all ORM interaction. The _ctrl_ prefix avoids a naming collision
    // with the identically-named CRUD functions in the model file.
    // ------------------------------------------------------------------

    // indx
    fprintf(controller_file, "// List all %s resources\n", model_name);
    fprintf(controller_file, "ControllerResult* indx_%s() {\n", lowercase_name);
    fprintf(controller_file, "    return indx(\"%s\");\n", model_name);
    fprintf(controller_file, "}\n\n");

    // view
    fprintf(controller_file, "// View a single %s by ID\n", model_name);
    fprintf(controller_file, "ControllerResult* view_ctrl_%s(int id) {\n", lowercase_name);
    fprintf(controller_file, "    return view(\"%s\", id);\n", model_name);
    fprintf(controller_file, "}\n\n");

    // create
    fprintf(controller_file, "// Create a new %s\n", model_name);
    fprintf(controller_file, "ControllerResult* create_ctrl_%s(char *data) {\n", lowercase_name);
    fprintf(controller_file, "    return create(\"%s\", data);\n", model_name);
    fprintf(controller_file, "}\n\n");

    // update
    fprintf(controller_file, "// Update an existing %s\n", model_name);
    fprintf(controller_file, "ControllerResult* update_ctrl_%s(int id, char *data) {\n", lowercase_name);
    fprintf(controller_file, "    return update(\"%s\", id, data);\n", model_name);
    fprintf(controller_file, "}\n\n");

    // replace (PUT)
    fprintf(controller_file, "// Fully replace a %s\n", model_name);
    fprintf(controller_file, "ControllerResult* replace_ctrl_%s(int id, char *data) {\n", lowercase_name);
    fprintf(controller_file, "    return replace(\"%s\", id, data);\n", model_name);
    fprintf(controller_file, "}\n");

    // destroy
    fprintf(controller_file, "// Delete a %s\n", model_name);
    fprintf(controller_file, "ControllerResult* destroy_ctrl_%s(int id) {\n", lowercase_name);
    fprintf(controller_file, "    return destroy(\"%s\", id);\n", model_name);
    fprintf(controller_file, "}\n");

    fclose(controller_file);
    free(lowercase_name);
    printf("Controller code generated for %s.\n", model_name);
}