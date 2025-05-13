#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stdio.h>

/**
 * @brief Get the absolute path to the project root directory
 * 
 * @param buffer Buffer to store the path
 * @param size Size of the buffer
 * @return int 0 on success, -1 on failure
 */
int get_project_root(char *buffer, size_t size);

/**
 * @brief Join the project root with a path to create a complete path
 * 
 * @param buffer Buffer to store the result
 * @param size Size of the buffer
 * @param path Path to join with the project root
 * @return int 0 on success, -1 on failure
 */
int join_project_path(char *buffer, size_t size, const char *path);

#endif // PATH_UTILS_H
