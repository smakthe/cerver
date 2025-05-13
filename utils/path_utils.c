#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "path_utils.h"

/**
 * @brief Get the absolute path to the project root directory
 * 
 * @param buffer Buffer to store the path
 * @param size Size of the buffer
 * @return int 0 on success, -1 on failure
 */
int get_project_root(char *buffer, size_t size) {
    char temp[PATH_MAX];
    
    // Get the current working directory
    if (getcwd(temp, sizeof(temp)) == NULL) {
        perror("Error getting current working directory");
        return -1;
    }
    
    // Copy the result to the output buffer
    strncpy(buffer, temp, size);
    buffer[size - 1] = '\0'; // Ensure null termination
    
    return 0;
}

/**
 * @brief Join the project root with a path to create a complete path
 * 
 * @param buffer Buffer to store the result
 * @param size Size of the buffer
 * @param path Path to join with the project root
 * @return int 0 on success, -1 on failure
 */
int join_project_path(char *buffer, size_t size, const char *path) {
    char root[PATH_MAX];
    
    if (get_project_root(root, sizeof(root)) != 0) {
        return -1;
    }
    
    snprintf(buffer, size, "%s/%s", root, path);
    return 0;
}
