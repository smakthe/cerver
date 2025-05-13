#ifndef B_PLUS_TREE_H
#define B_PLUS_TREE_H

#include <stddef.h> // For NULL

// --- Configuration ---
// MAX_KEYS defines the maximum number of keys in a node.
// The order of the tree is MAX_KEYS + 1.
// MIN_KEYS defines the minimum number of keys (except for the root).
#define MAX_KEYS 4
#define MIN_KEYS (MAX_KEYS / 2)

// --- Structures ---

// Represents a node in the B+ Tree
typedef struct BPlusTreeNode {
    int keys[MAX_KEYS];                     // Array to store keys
    long file_offsets[MAX_KEYS];            // Stores file offsets corresponding to keys (only used in leaf nodes)
    struct BPlusTreeNode *children[MAX_KEYS + 1]; // Array of pointers to child nodes (used in internal nodes)
    int num_keys;                           // Current number of keys in the node
    int is_leaf;                            // Flag: 1 if the node is a leaf, 0 otherwise
    struct BPlusTreeNode *parent;           // Pointer to the parent node (facilitates splitting/merging)
    struct BPlusTreeNode *next;             // Pointer to the next leaf node (for range scans)
} BPlusTreeNode;

// Represents the B+ Tree itself
typedef struct BPlusTree {
    BPlusTreeNode *root;                    // Pointer to the root node of the tree
    int order;                              // Order of the tree (MAX_KEYS + 1)
} BPlusTree;

// --- Function Prototypes ---

// Initialization
BPlusTree *initialize_tree();               // Creates and returns a new, empty B+ Tree
BPlusTreeNode *create_new_node(int is_leaf);// Creates and returns a new, empty B+ Tree node

// Insertion
// Inserts a key and its associated file offset into the tree.
void insert_key(BPlusTree *tree, int key, long file_offset);

// Search
// Searches for a key in the tree.
// Returns the associated file offset if found, otherwise returns -1.
long search_key(BPlusTree *tree, int key);

// Deletion
// Deletes a key (and its associated entry) from the tree.
void delete_key(BPlusTree *tree, int key);

// Utility (Optional - for debugging or specific needs)
void print_tree(BPlusTree *tree);           // Prints a representation of the tree structure (for debugging)

// Cleanup
void free_tree_nodes(BPlusTreeNode *node);  // Recursively frees memory allocated for nodes
void destroy_tree(BPlusTree *tree);         // Frees all memory associated with the tree

#endif // B_PLUS_TREE_H
