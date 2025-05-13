#ifndef SCAFFOLD_MODEL_H
#define SCAFFOLD_MODEL_H

typedef struct {
    char *name;
    char *type;
} Attribute;

typedef struct {
    const char *name;
    Attribute *attrs;
    int attr_count;
} ScaffoldModel;

void generate_model_code(ScaffoldModel *model);
void scaffold_model(const char *model_name, const char *attributes[], const char *types[], int attr_count);

#endif