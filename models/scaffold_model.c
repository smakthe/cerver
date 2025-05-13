#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "scaffold_model.h"
#include "../database/application/orm.h"

#define MAX_ATTRS 100
#define MAX_ATTR_NAME 50
#define MAX_ATTR_TYPE 50
#define MAX_MODEL_NAME 100

// Convert string to lowercase
char* to_lowercase(const char* str) {
    if (!str) return NULL;
    
    char* result = strdup(str);
    for (int i = 0; result[i]; i++) {
        result[i] = tolower(result[i]);
    }
    return result;
}

// Function to create a new model
void generate_model_code(ScaffoldModel *model) {
    // Convert model name to lowercase
    char* lowercase_name = to_lowercase(model->name);
    
    // Create directory for the resource if it doesn't exist
    char resource_dir[256];
    snprintf(resource_dir, sizeof(resource_dir), "/Users/somak/cerver/scaffolded_resources/%s", lowercase_name);
    
    // Create the directory (mkdir -p equivalent)
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", resource_dir);
    system(mkdir_cmd);
    
    // Create the model file inside the resource directory
    FILE *model_file;
    char model_filename[512];
    snprintf(model_filename, sizeof(model_filename), "%s/%s.c", resource_dir, lowercase_name);

    model_file = fopen(model_filename, "w");
    if (model_file == NULL) {
        printf("Error opening file for model generation\n");
        free(lowercase_name);
        return;
    }
    
    printf("Creating model file: %s\n", model_filename);

    // Include necessary headers
    fprintf(model_file, "#include <stdio.h>\n");
    fprintf(model_file, "#include <stdlib.h>\n");
    fprintf(model_file, "#include <string.h>\n");
    fprintf(model_file, "#include \"../database/application/orm.h\"\n\n");

    // Define the model structure (for in-memory use)
    fprintf(model_file, "typedef struct {\n");
    for (int i = 0; i < model->attr_count; i++) {
        fprintf(model_file, "    %s %s;\n", model->attrs[i].type, model->attrs[i].name);
    }
    fprintf(model_file, "} %s;\n\n", model->name);

    // Create function
    fprintf(model_file, "int create_%s(%s *new_%s) {\n", model->name, model->name, model->name);
    fprintf(model_file, "    // Get model schema reference\n");
    fprintf(model_file, "    extern Model *get_model_schema(const char *model_name);\n");
    fprintf(model_file, "    Model *model_schema = get_model_schema(\"%s\");\n", model->name);
    fprintf(model_file, "    if (!model_schema) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: Model schema for %s not found\\n\");\n", model->name);
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Create a new model instance\n");
    fprintf(model_file, "    ModelInstance *instance = create_new_instance(model_schema);\n");
    fprintf(model_file, "    if (!instance) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: Failed to create new instance\\n\");\n");
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Set field values\n");
    for (int i = 0; i < model->attr_count; i++) {
        // Convert int fields to string for set_instance_field
        if (strcmp(model->attrs[i].type, "int") == 0) {
            fprintf(model_file, "    char %s_str[32];\n", model->attrs[i].name);
            fprintf(model_file, "    snprintf(%s_str, sizeof(%s_str), \"%%d\", new_%s->%s);\n", model->attrs[i].name, model->attrs[i].name, model->name, model->attrs[i].name);
            fprintf(model_file, "    set_instance_field(instance, %d, %s_str);\n", i, model->attrs[i].name);
        } else {
            fprintf(model_file, "    set_instance_field(instance, %d, new_%s->%s);\n", i, model->name, model->attrs[i].name);
        }
    }
    
    fprintf(model_file, "\n    // Save the instance to the database\n");
    fprintf(model_file, "    int result = save_model_instance(instance);\n");
    fprintf(model_file, "    free_model_instance(instance);\n");
    fprintf(model_file, "    return result;\n");
    fprintf(model_file, "}\n\n");

    // View function
    fprintf(model_file, "int view_%s(int id, %s *out_%s) {\n", model->name, model->name, model->name);
    fprintf(model_file, "    // Get model schema reference\n");
    fprintf(model_file, "    extern Model *get_model_schema(const char *model_name);\n");
    fprintf(model_file, "    Model *model_schema = get_model_schema(\"%s\");\n", model->name);
    fprintf(model_file, "    if (!model_schema) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: Model schema for %s not found\\n\");\n", model->name);
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Find the model instance by primary key\n");
    fprintf(model_file, "    ModelInstance *instance = find_model_by_primary_key(model_schema, id);\n");
    fprintf(model_file, "    if (!instance) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: %s with ID %%d not found\\n\", id);\n", model->name);
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Extract field values\n");
    for (int i = 0; i < model->attr_count; i++) {
        if (strcmp(model->attrs[i].type, "int") == 0) {
            fprintf(model_file, "    const char *val_%s = instance->data[%d];\n", model->attrs[i].name, i);
            fprintf(model_file, "    out_%s->%s = val_%s ? atoi(val_%s) : 0;\n", model->name, model->attrs[i].name, model->attrs[i].name, model->attrs[i].name);
        } else {
            fprintf(model_file, "    const char *val_%s = instance->data[%d];\n", model->attrs[i].name, i);
            fprintf(model_file, "    if (val_%s) strncpy(out_%s->%s, val_%s, sizeof(out_%s->%s)-1);\n", model->attrs[i].name, model->name, model->attrs[i].name, model->attrs[i].name, model->name, model->attrs[i].name);
        }
    }
    
    fprintf(model_file, "    free_model_instance(instance);\n");
    fprintf(model_file, "    return 0;\n");
    fprintf(model_file, "}\n\n");

    // Update function
    fprintf(model_file, "int update_%s(int id, %s *updated_%s) {\n", model->name, model->name, model->name);
    fprintf(model_file, "    // Get model schema reference\n");
    fprintf(model_file, "    extern Model *get_model_schema(const char *model_name);\n");
    fprintf(model_file, "    Model *model_schema = get_model_schema(\"%s\");\n", model->name);
    fprintf(model_file, "    if (!model_schema) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: Model schema for %s not found\\n\");\n", model->name);
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Find the model instance by primary key\n");
    fprintf(model_file, "    ModelInstance *instance = find_model_by_primary_key(model_schema, id);\n");
    fprintf(model_file, "    if (!instance) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: %s with ID %%d not found\\n\", id);\n", model->name);
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Update field values\n");
    for (int i = 0; i < model->attr_count; i++) {
        if (strcmp(model->attrs[i].type, "int") == 0) {
            fprintf(model_file, "    char %s_str[32];\n", model->attrs[i].name);
            fprintf(model_file, "    snprintf(%s_str, sizeof(%s_str), \"%%d\", updated_%s->%s);\n", model->attrs[i].name, model->attrs[i].name, model->name, model->attrs[i].name);
            fprintf(model_file, "    set_instance_field(instance, %d, %s_str);\n", i, model->attrs[i].name);
        } else {
            fprintf(model_file, "    set_instance_field(instance, %d, updated_%s->%s);\n", i, model->name, model->attrs[i].name);
        }
    }
    
    fprintf(model_file, "    // Save the updated instance\n");
    fprintf(model_file, "    int result = save_model_instance(instance);\n");
    fprintf(model_file, "    free_model_instance(instance);\n");
    fprintf(model_file, "    return result;\n");
    fprintf(model_file, "}\n\n");

    // Destroy function
    fprintf(model_file, "int destroy_%s(int id) {\n", model->name);
    fprintf(model_file, "    // Get model schema reference\n");
    fprintf(model_file, "    extern Model *get_model_schema(const char *model_name);\n");
    fprintf(model_file, "    Model *model_schema = get_model_schema(\"%s\");\n", model->name);
    fprintf(model_file, "    if (!model_schema) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: Model schema for %s not found\\n\");\n", model->name);
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Find the model instance by primary key\n");
    fprintf(model_file, "    ModelInstance *instance = find_model_by_primary_key(model_schema, id);\n");
    fprintf(model_file, "    if (!instance) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: %s with ID %%d not found\\n\", id);\n", model->name);
    fprintf(model_file, "        return -1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Delete the instance\n");
    fprintf(model_file, "    int result = delete_model_instance(instance);\n");
    fprintf(model_file, "    free_model_instance(instance);\n");
    fprintf(model_file, "    return result;\n");
    fprintf(model_file, "}\n\n");

    // Helper function to get model schema
    fprintf(model_file, "// Helper function to get the model schema from the ORM\n");
    fprintf(model_file, "Model *get_model_schema(const char *model_name) {\n");
    fprintf(model_file, "    extern Database *global_db;\n");
    fprintf(model_file, "    if (!global_db) {\n");
    fprintf(model_file, "        fprintf(stderr, \"Error: Database not initialized\\n\");\n");
    fprintf(model_file, "        return NULL;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // This is a simplified placeholder\n");
    fprintf(model_file, "    // In a real implementation, this would look up the model in a model registry\n");
    fprintf(model_file, "    // For now, create a model definition on the fly\n");
    fprintf(model_file, "    static Field fields[%d];\n", model->attr_count);
    fprintf(model_file, "    static int fields_initialized = 0;\n\n");
    
    fprintf(model_file, "    if (!fields_initialized) {\n");
    for (int i = 0; i < model->attr_count; i++) {
        fprintf(model_file, "        fields[%d].name = \"%s\";\n", i, model->attrs[i].name);
        fprintf(model_file, "        fields[%d].type = \"%s\";\n", i, model->attrs[i].type);
        // Make the first field the primary key
        if (i == 0) {
            fprintf(model_file, "        fields[%d].is_primary = 1;\n", i);
        } else {
            fprintf(model_file, "        fields[%d].is_primary = 0;\n", i);
        }
        fprintf(model_file, "        fields[%d].is_foreign_key = 0;\n", i);
        fprintf(model_file, "        fields[%d].referenced_table = NULL;\n", i);
        fprintf(model_file, "        fields[%d].referenced_column = NULL;\n", i);
    }
    fprintf(model_file, "        fields_initialized = 1;\n");
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    // Define the model with the ORM\n");
    fprintf(model_file, "    static Model *model = NULL;\n");
    fprintf(model_file, "    if (!model) {\n");
    fprintf(model_file, "        model = define_model(\"%s\", fields, %d, NULL, 0);\n", model->name, model->attr_count);
    fprintf(model_file, "    }\n\n");
    
    fprintf(model_file, "    return model;\n");
    fprintf(model_file, "}\n");

    fclose(model_file);
    printf("Model code generated for %s (integrated with database).\n", model->name);
}

// Function to generate the scaffold model based on user input
void scaffold_model(const char *model_name, const char *attributes[], const char *types[], int attr_count) {
    // Create a new model
    ScaffoldModel model;
    model.name = strdup(model_name);
    model.attrs = malloc(sizeof(Attribute) * attr_count);
    model.attr_count = attr_count;

    // Assign attributes and types
    for (int i = 0; i < attr_count; i++) {
        model.attrs[i].name = strdup(attributes[i]);
        model.attrs[i].type = strdup(types[i]);
    }

    // Generate the model code
    generate_model_code(&model);
    
    // Free all allocated memory
    free((void*)model.name);
    for (int i = 0; i < attr_count; i++) {
        free(model.attrs[i].name);
        free(model.attrs[i].type);
    }
    free(model.attrs);
}