#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "b_plus_tree.h"

// --- Forward declarations for internal helper functions ---
BPlusTreeNode *find_leaf_node(BPlusTreeNode *node, int key);
void insert_into_leaf(BPlusTreeNode *leaf, int key, long file_offset);
void insert_into_parent(BPlusTree* tree, BPlusTreeNode *left, int key, BPlusTreeNode *right);
void insert_into_node(BPlusTreeNode *node, int index, int key, BPlusTreeNode *right_child);
// void split_leaf_node(BPlusTree* tree, BPlusTreeNode *leaf); // Integrated into insert_key
// void split_internal_node(BPlusTree* tree, BPlusTreeNode *node); // Integrated into insert_into_parent
void delete_entry(BPlusTree *tree, BPlusTreeNode *node, int key);
void handle_underflow(BPlusTree *tree, BPlusTreeNode *node);
// int get_neighbor_index(BPlusTreeNode *node); // Integrated into handle_underflow
// void borrow_from_sibling(BPlusTreeNode *node, BPlusTreeNode *sibling, int neighbor_index, int is_predecessor); // Integrated
void merge_nodes(BPlusTree *tree, BPlusTreeNode *left_node, BPlusTreeNode *right_node, int k_prime_index, int k_prime);


// --- Initialization ---

/**
 * @brief Initializes a new B+ Tree.
 * Allocates memory for the tree structure and its root node (which starts as a leaf).
 * @return Pointer to the newly created BPlusTree, or NULL on failure.
 */
BPlusTree *initialize_tree() {
    BPlusTree *tree = (BPlusTree *)malloc(sizeof(BPlusTree));
    if (!tree) {
        perror("Failed to allocate memory for BPlusTree");
        return NULL;
    }
    tree->root = create_new_node(1); // Root starts as a leaf
    if (!tree->root) {
        perror("Failed to allocate memory for root node");
        free(tree);
        return NULL;
    }
    tree->order = MAX_KEYS + 1;
    tree->root->parent = NULL; // Root has no parent initially
    return tree;
}

/**
 * @brief Creates a new B+ Tree node.
 * Allocates memory and initializes basic properties (leaf status, key count, pointers).
 * @param is_leaf 1 if the node should be a leaf, 0 otherwise.
 * @return Pointer to the newly created BPlusTreeNode, or NULL on failure.
 */
BPlusTreeNode *create_new_node(int is_leaf) {
    BPlusTreeNode *node = (BPlusTreeNode *)malloc(sizeof(BPlusTreeNode));
    if (!node) {
         perror("Failed to allocate memory for BPlusTreeNode");
        return NULL;
    }
    node->is_leaf = is_leaf;
    node->num_keys = 0;
    node->parent = NULL; // Initialize parent to NULL
    node->next = NULL;   // Initialize next pointer (for leaves) to NULL
    // Initialize children pointers to NULL
    for (int i = 0; i < MAX_KEYS + 1; i++) {
        node->children[i] = NULL;
    }
    // No need to initialize keys/offsets explicitly as num_keys is 0
    return node;
}

// --- Search ---

/**
 * @brief Finds the leaf node where a given key should reside.
 * Traverses the tree from the given node down to the appropriate leaf.
 * @param node The starting node (usually the root).
 * @param key The key to search for.
 * @return Pointer to the leaf node where the key is or should be inserted, or NULL if tree is empty/invalid.
 */
BPlusTreeNode *find_leaf_node(BPlusTreeNode *node, int key) {
    if (!node) return NULL;
    BPlusTreeNode *current = node;
    // Traverse down the tree until a leaf node is reached
    while (!current->is_leaf) {
        int i = 0;
        // Find the first key in the internal node that is >= the search key
        // Or go to the rightmost child if key is larger than all keys
        while (i < current->num_keys && key >= current->keys[i]) {
            i++;
        }
        // Follow the appropriate child pointer
        current = current->children[i];
        if (!current) return NULL; // Should not happen in a valid tree
    }
    return current; // Return the leaf node found
}

/**
 * @brief Searches for a key within the B+ Tree.
 * Finds the appropriate leaf node and performs a linear search within that node.
 * @param tree Pointer to the BPlusTree.
 * @param key The key to search for.
 * @return The file offset associated with the key if found, otherwise -1.
 */
long search_key(BPlusTree *tree, int key) {
    if (!tree || !tree->root) return -1; // Handle empty or invalid tree

    // Find the leaf node where the key might exist
    BPlusTreeNode *leaf = find_leaf_node(tree->root, key);
    if (!leaf) return -1; // Should not happen if root exists

    // Linear search within the leaf node for the key
    for (int i = 0; i < leaf->num_keys; i++) {
        if (leaf->keys[i] == key) {
            return leaf->file_offsets[i]; // Key found, return its associated offset
        }
    }
    return -1; // Key not found in the leaf node
}

// --- Insertion ---

/**
 * @brief Inserts a key and its associated file offset into the B+ Tree.
 * Handles finding the correct leaf, inserting, and splitting nodes if necessary,
 * propagating splits up the tree.
 * @param tree Pointer to the BPlusTree.
 * @param key The key to insert.
 * @param file_offset The file offset associated with the key.
 */
void insert_key(BPlusTree *tree, int key, long file_offset) {
    if (!tree || !tree->root) return; // Cannot insert into an invalid tree

    // Special case: Tree is completely empty (root is a leaf with 0 keys)
    if (tree->root->is_leaf && tree->root->num_keys == 0) {
         insert_into_leaf(tree->root, key, file_offset);
         return;
    }

    // Find the appropriate leaf node for insertion
    BPlusTreeNode *leaf = find_leaf_node(tree->root, key);
    if (!leaf) return; // Should not happen

    // Optional: Check for duplicate keys if they are not allowed
    // for (int i = 0; i < leaf->num_keys; i++) {
    //     if (leaf->keys[i] == key) {
    //         fprintf(stderr, "Warning: Duplicate key %d insertion attempted.\n", key);
    //         // Decide how to handle duplicates: update offset, return error, etc.
    //         // leaf->file_offsets[i] = file_offset; // Example: Update offset
    //         return; // Or return an error code
    //     }
    // }

    // If the leaf node has space, insert directly
    if (leaf->num_keys < MAX_KEYS) {
        insert_into_leaf(leaf, key, file_offset);
    } else {
        // Leaf node is full, needs splitting
        // Create temporary arrays large enough to hold the new key + existing keys
        int temp_keys[MAX_KEYS + 1];
        long temp_offsets[MAX_KEYS + 1];

        // Find the position where the new key should be inserted and copy existing keys/offsets
        int i = 0;
        while (i < MAX_KEYS && key > leaf->keys[i]) {
            temp_keys[i] = leaf->keys[i];
            temp_offsets[i] = leaf->file_offsets[i];
            i++;
        }
        // Insert the new key and offset
        temp_keys[i] = key;
        temp_offsets[i] = file_offset;
        // Copy the remaining keys/offsets
        while(i < MAX_KEYS) {
            temp_keys[i+1] = leaf->keys[i];
            temp_offsets[i+1] = leaf->file_offsets[i];
            i++;
        }

        // Create a new leaf node to become the right sibling
        BPlusTreeNode *new_leaf = create_new_node(1);
        if (!new_leaf) return; // Allocation failed
        new_leaf->parent = leaf->parent; // New leaf shares the same parent initially

        // Determine the split point (ceiling of (MAX_KEYS+1)/2)
        int split_point = (MAX_KEYS + 1) / 2;

        // Redistribute keys/offsets between the original leaf and the new leaf
        leaf->num_keys = split_point;
        for(i = 0; i < split_point; i++) {
            leaf->keys[i] = temp_keys[i];
            leaf->file_offsets[i] = temp_offsets[i];
        }

        new_leaf->num_keys = (MAX_KEYS + 1) - split_point;
        for(i = 0; i < new_leaf->num_keys; i++) {
            new_leaf->keys[i] = temp_keys[split_point + i];
            new_leaf->file_offsets[i] = temp_offsets[split_point + i];
        }

        // Link the new leaf into the list of leaf nodes
        new_leaf->next = leaf->next;
        leaf->next = new_leaf;

        // Insert the first key of the new leaf into the parent node
        insert_into_parent(tree, leaf, new_leaf->keys[0], new_leaf);
    }
}

/**
 * @brief Inserts a key and file offset into a leaf node that is known to have space.
 * Shifts existing keys/offsets to make room and inserts the new entry in sorted order.
 * @param leaf Pointer to the leaf node.
 * @param key The key to insert.
 * @param file_offset The file offset to insert.
 */
void insert_into_leaf(BPlusTreeNode *leaf, int key, long file_offset) {
    int i = leaf->num_keys - 1;
    // Find the correct position to insert the key while maintaining sorted order
    // Shift existing keys and offsets to the right
    while (i >= 0 && key < leaf->keys[i]) {
        leaf->keys[i + 1] = leaf->keys[i];
        leaf->file_offsets[i + 1] = leaf->file_offsets[i];
        i--;
    }
    // Insert the new key and offset at the correct position
    leaf->keys[i + 1] = key;
    leaf->file_offsets[i + 1] = file_offset;
    leaf->num_keys++; // Increment the key count
}

/**
 * @brief Inserts a new key and child pointer into a parent node after a split.
 * Handles splitting the parent node if it's full, potentially creating a new root.
 * @param tree Pointer to the BPlusTree.
 * @param left Pointer to the existing child node (the left node after the split).
 * @param key The key to insert into the parent (usually the first key of the 'right' node).
 * @param right Pointer to the new child node created during the split (the right node).
 */
void insert_into_parent(BPlusTree* tree, BPlusTreeNode *left, int key, BPlusTreeNode *right) {
    BPlusTreeNode *parent = left->parent;

    // Case 1: The 'left' node was the root. A new root must be created.
    if (parent == NULL) {
        BPlusTreeNode *new_root = create_new_node(0); // New root is an internal node
        if (!new_root) return; // Allocation failed
        new_root->keys[0] = key;         // The key that separated left and right
        new_root->children[0] = left;    // Old root becomes the left child
        new_root->children[1] = right;   // New node becomes the right child
        new_root->num_keys = 1;
        // Update parent pointers of the children
        left->parent = new_root;
        right->parent = new_root;
        tree->root = new_root;           // Assign the new root to the tree
        return;
    }

    // Case 2: Parent exists. Find the position for the new key/child pointer.
    int index = 0; // Index of the pointer *before* which the new key should be inserted
    while (index < parent->num_keys && parent->children[index] != left) {
        index++;
    }

    // Case 2a: Parent has space for the new key and child.
    if (parent->num_keys < MAX_KEYS) {
        insert_into_node(parent, index, key, right);
    }
    // Case 2b: Parent is full. Split the parent node.
    else {
        // Create temporary arrays to hold keys and children during split
        int temp_keys[MAX_KEYS + 1];
        BPlusTreeNode *temp_children[MAX_KEYS + 2];

        // Copy keys/children from the parent up to the insertion point 'index'
        for (int i = 0; i < index; i++) {
             temp_keys[i] = parent->keys[i];
             temp_children[i] = parent->children[i];
        }
         temp_children[index] = parent->children[index]; // This is the 'left' child

        // Insert the new key and the 'right' child pointer
        temp_keys[index] = key;
        temp_children[index + 1] = right;

        // Copy the remaining keys/children from the parent
        for (int i = index; i < parent->num_keys; i++) {
            temp_keys[i + 1] = parent->keys[i];
            temp_children[i + 2] = parent->children[i + 1];
        }

        // Create a new internal node to be the right sibling after the split
        BPlusTreeNode *new_internal = create_new_node(0);
        if(!new_internal) return; // Allocation failed
        new_internal->parent = parent->parent; // Share the same grandparent initially

        // Determine the key to be promoted to the grandparent
        int split_point_key_index = MAX_KEYS / 2; // Index of the key to promote (middle key)
        int k_prime = temp_keys[split_point_key_index];

        // Update the original node (parent) - it becomes the left node after split
        parent->num_keys = split_point_key_index;
        for(int i=0; i < parent->num_keys; i++) {
            parent->keys[i] = temp_keys[i];
            parent->children[i] = temp_children[i];
            // No need to update parent pointers here, they were already correct
        }
        // Update the last child pointer for the modified parent node
        parent->children[parent->num_keys] = temp_children[parent->num_keys];


        // Fill the new internal node (the right node after split)
        new_internal->num_keys = MAX_KEYS - parent->num_keys; // Keys after the promoted key
        for(int i=0; i < new_internal->num_keys; i++) {
            // Keys start from index split_point_key_index + 1 in temp_keys
            new_internal->keys[i] = temp_keys[split_point_key_index + 1 + i];
            // Children start from index split_point_key_index + 1 in temp_children
            new_internal->children[i] = temp_children[split_point_key_index + 1 + i];
            // Update the parent pointer of these children to the new internal node
            if (new_internal->children[i]) new_internal->children[i]->parent = new_internal;
        }
        // Update the last child pointer for the new internal node
        new_internal->children[new_internal->num_keys] = temp_children[MAX_KEYS + 1];
         if (new_internal->children[new_internal->num_keys]) {
             new_internal->children[new_internal->num_keys]->parent = new_internal;
         }


        // Recursively insert the promoted key (k_prime) into the grandparent
        insert_into_parent(tree, parent, k_prime, new_internal);
    }
}

/**
 * @brief Inserts a key and a right child pointer into an internal node known to have space.
 * Shifts existing keys/pointers to make room.
 * @param node Pointer to the internal node.
 * @param index The index in the parent's keys array where the new key should be inserted.
 * The new child pointer (`right_child`) will be placed at `children[index + 1]`.
 * @param key The key to insert.
 * @param right_child Pointer to the child node that should be to the right of the inserted key.
 */
void insert_into_node(BPlusTreeNode *node, int index, int key, BPlusTreeNode *right_child) {
     // Shift keys and children pointers to the right to make space for the new entry
    for (int i = node->num_keys; i > index; i--) {
        node->keys[i] = node->keys[i - 1];
        node->children[i + 1] = node->children[i];
    }
    // Insert the new key and the right child pointer at the correct position
    node->keys[index] = key;
    node->children[index + 1] = right_child;
    // Update the parent pointer of the newly inserted child
    if (right_child) right_child->parent = node;
    node->num_keys++; // Increment the key count
}


// --- Deletion ---

/**
 * @brief Initiates the deletion of a key from the B+ Tree.
 * Finds the leaf containing the key and calls delete_entry.
 * @param tree Pointer to the BPlusTree.
 * @param key The key to delete.
 */
void delete_key(BPlusTree *tree, int key) {
    if (!tree || !tree->root) return; // Cannot delete from an invalid tree

    // Find the leaf node where the key should exist
    BPlusTreeNode *leaf = find_leaf_node(tree->root, key);
    if (!leaf) {
        // fprintf(stderr, "Key %d not found (leaf not reachable).\n", key);
        return; // Key not found or tree structure issue
    }

    // Find the specific index of the key within the leaf node
    int key_index = -1;
    for(int i=0; i<leaf->num_keys; i++) {
        if(leaf->keys[i] == key) {
            key_index = i;
            break;
        }
    }

    // If the key was not found in the leaf node
    if (key_index == -1) {
        // fprintf(stderr, "Key %d not found in leaf node for deletion.\n", key);
        return;
    }

    // Call the recursive deletion function starting from the leaf
    delete_entry(tree, leaf, key);
}

/**
 * @brief Deletes an entry (key and associated data/pointer) from a node.
 * Handles shifting elements, and triggers underflow handling if necessary.
 * This function is primarily designed for leaf node deletion initially, but the
 * underflow handling part works recursively for internal nodes as well.
 * @param tree Pointer to the BPlusTree.
 * @param node Pointer to the node from which the key should be deleted.
 * @param key The key to delete.
 */
void delete_entry(BPlusTree *tree, BPlusTreeNode *node, int key) {
    // Find the index of the key to delete within the node
    int index = 0;
    while (index < node->num_keys && node->keys[index] < key) {
        index++;
    }

    // Key should be at 'index' if this function is called correctly after checks
    if (index == node->num_keys || node->keys[index] != key) {
         // This indicates an issue, key should have been found before calling this
         fprintf(stderr, "Internal Error: Key %d not found in node %p during delete_entry.\n", key, (void*)node);
         return;
     }

    // Remove the key (and associated offset if it's a leaf node) by shifting elements left
    for (int i = index; i < node->num_keys - 1; i++) {
        node->keys[i] = node->keys[i + 1];
        if (node->is_leaf) {
            node->file_offsets[i] = node->file_offsets[i + 1];
        }
        // Note: For internal nodes, deleting a key also requires shifting children pointers.
        // This is handled implicitly when merging/borrowing pulls down a parent key.
        // Direct deletion from internal nodes is more complex in B+ trees and usually involves
        // replacing the key with its successor from a leaf and deleting from the leaf.
        // This implementation focuses on leaf deletion triggering potential parent adjustments.
    }
    node->num_keys--; // Decrement the key count

    // --- Underflow Check and Handling ---
    // Check if the node has fallen below the minimum number of keys required
    // The root node is allowed to have fewer than MIN_KEYS.
    if (node != tree->root && node->num_keys < MIN_KEYS) {
        handle_underflow(tree, node);
    }
    // Special case: If the root node becomes empty after deletion/merging
    else if (tree->root->num_keys == 0 && !tree->root->is_leaf) {
        // If the root is an internal node and becomes empty, its only child becomes the new root
        BPlusTreeNode *old_root = tree->root;
        tree->root = old_root->children[0]; // Promote the first (and only) child
        if (tree->root) tree->root->parent = NULL; // New root has no parent
        free(old_root); // Free the old empty root
    } else if (tree->root->num_keys == 0 && tree->root->is_leaf) {
        // If the root is a leaf and becomes empty, the tree is now completely empty.
        // We keep the empty root leaf node.
    }
}

/**
 * @brief Handles the underflow condition in a node after deletion.
 * Attempts to borrow a key from a sibling node. If borrowing is not possible,
 * it merges the node with a sibling. Recursively handles underflow in the parent if merging occurs.
 * @param tree Pointer to the BPlusTree.
 * @param node Pointer to the node that has underflowed.
 */
void handle_underflow(BPlusTree *tree, BPlusTreeNode *node) {
    // No underflow if node is root or has enough keys
    if (!node || node == tree->root || node->num_keys >= MIN_KEYS) {
        return;
    }

    BPlusTreeNode *parent = node->parent;
    if (!parent) {
         fprintf(stderr, "Internal Error: Underflow node %p has no parent.\n", (void*)node);
         return; // Should not happen if not root
    }

    // Find the index of 'node' within its parent's children array
    int node_index_in_parent = 0;
    while(node_index_in_parent <= parent->num_keys && parent->children[node_index_in_parent] != node) {
        node_index_in_parent++;
    }
     if(node_index_in_parent > parent->num_keys) {
         fprintf(stderr, "Internal Error: Could not find node %p in parent %p children.\n", (void*)node, (void*)parent);
         return; // Error case
     }

    // --- Attempt to Borrow from Left Sibling ---
    if (node_index_in_parent > 0) { // Check if a left sibling exists
        BPlusTreeNode *left_sibling = parent->children[node_index_in_parent - 1];
        // Check if the left sibling has more than the minimum number of keys
        if (left_sibling->num_keys > MIN_KEYS) {
            int k_prime_index = node_index_in_parent - 1; // Index of key in parent separating node and left sibling

            // Shift elements in 'node' to the right to make space for the borrowed element
            for(int i = node->num_keys; i > 0; i--) {
                node->keys[i] = node->keys[i-1];
                if(node->is_leaf) node->file_offsets[i] = node->file_offsets[i-1];
                // Shift children pointers as well if it's an internal node
                else node->children[i+1] = node->children[i];
            }
             // Shift the first child pointer if internal node
             if (!node->is_leaf) node->children[1] = node->children[0];


            if (node->is_leaf) {
                // Borrow the last key/offset from the left sibling
                node->keys[0] = left_sibling->keys[left_sibling->num_keys - 1];
                node->file_offsets[0] = left_sibling->file_offsets[left_sibling->num_keys - 1];
                // Update the separating key in the parent to the new first key of 'node'
                parent->keys[k_prime_index] = node->keys[0];
            } else { // Internal node borrowing
                // Borrow the separating key from the parent
                node->keys[0] = parent->keys[k_prime_index];
                // Borrow the last child pointer from the left sibling
                node->children[0] = left_sibling->children[left_sibling->num_keys];
                // Update the parent pointer of the borrowed child
                if(node->children[0]) node->children[0]->parent = node;
                 // Update the separating key in the parent with the last key from the left sibling
                parent->keys[k_prime_index] = left_sibling->keys[left_sibling->num_keys - 1];
            }

            left_sibling->num_keys--; // Decrement key count in sibling
            node->num_keys++;       // Increment key count in node
            return; // Borrowing successful, underflow resolved
        }
    }

    // --- Attempt to Borrow from Right Sibling ---
    if (node_index_in_parent < parent->num_keys) { // Check if a right sibling exists
        BPlusTreeNode *right_sibling = parent->children[node_index_in_parent + 1];
        // Check if the right sibling has more than the minimum number of keys
        if (right_sibling->num_keys > MIN_KEYS) {
            int k_prime_index = node_index_in_parent; // Index of key in parent separating node and right sibling

            if (node->is_leaf) {
                 // Borrow the first key/offset from the right sibling
                node->keys[node->num_keys] = right_sibling->keys[0];
                node->file_offsets[node->num_keys] = right_sibling->file_offsets[0];
                // Update the separating key in the parent to the *new* first key of the right sibling
                parent->keys[k_prime_index] = right_sibling->keys[1];

                // Shift elements left in the right sibling
                for(int i = 0; i < right_sibling->num_keys - 1; i++) {
                     right_sibling->keys[i] = right_sibling->keys[i+1];
                     right_sibling->file_offsets[i] = right_sibling->file_offsets[i+1];
                }
            } else { // Internal node borrowing
                // Borrow the separating key from the parent
                node->keys[node->num_keys] = parent->keys[k_prime_index];
                // Borrow the first child pointer from the right sibling
                node->children[node->num_keys + 1] = right_sibling->children[0];
                // Update the parent pointer of the borrowed child
                if(node->children[node->num_keys + 1]) node->children[node->num_keys + 1]->parent = node;
                // Update the separating key in the parent with the first key from the right sibling
                parent->keys[k_prime_index] = right_sibling->keys[0];

                 // Shift keys and children left in the right sibling
                 right_sibling->children[0] = right_sibling->children[1]; // First child pointer shifts
                 for(int i = 0; i < right_sibling->num_keys - 1; i++) {
                     right_sibling->keys[i] = right_sibling->keys[i+1];
                     right_sibling->children[i+1] = right_sibling->children[i+2];
                 }
                 // The last child pointer slot is now unused
                 right_sibling->children[right_sibling->num_keys] = NULL;
            }

            right_sibling->num_keys--; // Decrement key count in sibling
            node->num_keys++;        // Increment key count in node
            return; // Borrowing successful, underflow resolved
        }
    }

    // --- Merge with a Sibling ---
    // If borrowing failed, we must merge the node with one of its siblings
    if (node_index_in_parent > 0) {
        // Merge with the left sibling: Merge 'node' INTO 'left_sibling'
        BPlusTreeNode *left_sibling = parent->children[node_index_in_parent - 1];
        int k_prime_index = node_index_in_parent - 1;
        int k_prime = parent->keys[k_prime_index]; // Key separating left_sibling and node
        merge_nodes(tree, left_sibling, node, k_prime_index, k_prime);
    } else {
        // Merge with the right sibling: Merge 'right_sibling' INTO 'node'
        BPlusTreeNode *right_sibling = parent->children[node_index_in_parent + 1];
        int k_prime_index = node_index_in_parent;
        int k_prime = parent->keys[k_prime_index]; // Key separating node and right_sibling
        merge_nodes(tree, node, right_sibling, k_prime_index, k_prime);
    }
}

/**
 * @brief Merges the 'right_node' into the 'left_node'.
 * This involves pulling down a key from the parent and moving all keys/pointers
 * from the right node into the left node. The right node is then freed.
 * Recursively handles underflow in the parent.
 * @param tree Pointer to the BPlusTree.
 * @param left_node The node that will receive the merged contents.
 * @param right_node The node whose contents will be merged; this node will be freed.
 * @param k_prime_index The index of the key in the parent node that separates left_node and right_node.
 * @param k_prime The value of the key in the parent that separates the nodes.
 */
void merge_nodes(BPlusTree *tree, BPlusTreeNode *left_node, BPlusTreeNode *right_node, int k_prime_index, int k_prime) {
    BPlusTreeNode *parent = left_node->parent; // Both nodes must share the same parent

    // If merging internal nodes, pull down the separating key from the parent into the left node
    if (!left_node->is_leaf) {
        left_node->keys[left_node->num_keys] = k_prime;
        left_node->num_keys++;
    }

    // Copy all keys and data/children pointers from the right_node to the end of the left_node
    for (int i = 0; i < right_node->num_keys; i++) {
        left_node->keys[left_node->num_keys] = right_node->keys[i];
        if (left_node->is_leaf) {
            left_node->file_offsets[left_node->num_keys] = right_node->file_offsets[i];
        } else { // Internal node: copy child pointer
            left_node->children[left_node->num_keys] = right_node->children[i];
            // Update the parent pointer of the moved child
            if (left_node->children[left_node->num_keys]) {
                left_node->children[left_node->num_keys]->parent = left_node;
            }
        }
        left_node->num_keys++;
    }

    // If internal nodes, copy the last child pointer from the right_node
    if (!left_node->is_leaf) {
        left_node->children[left_node->num_keys] = right_node->children[right_node->num_keys];
         if (left_node->children[left_node->num_keys]) {
            left_node->children[left_node->num_keys]->parent = left_node;
        }
    }

    // If merging leaf nodes, update the 'next' pointer of the left node
    if (left_node->is_leaf) {
        left_node->next = right_node->next;
    }

    // Remove the separating key (k_prime) and the pointer to right_node from the parent node
    // Shift keys and child pointers in the parent to the left
    for (int i = k_prime_index; i < parent->num_keys - 1; i++) {
        parent->keys[i] = parent->keys[i + 1];
        parent->children[i + 1] = parent->children[i + 2]; // Shift children pointers starting after the left_node
    }
    // Clear the last pointer slot in parent
    parent->children[parent->num_keys] = NULL;
    parent->num_keys--;

    // Free the memory allocated for the right_node, as it's now empty and merged
    free(right_node);

    // --- Check Parent for Underflow ---
    // After merging, the parent might have underflowed
    if (parent != tree->root && parent->num_keys < MIN_KEYS) {
        handle_underflow(tree, parent);
    }
    // Special case: If the root became empty after the merge (only happens if root had 1 key and 2 children before merge)
    else if (tree->root->num_keys == 0 && !tree->root->is_leaf) {
        // The merged node (left_node) becomes the new root
        BPlusTreeNode *old_root = tree->root;
        tree->root = left_node;
        left_node->parent = NULL; // New root has no parent
        free(old_root);
    }
}


// --- Cleanup ---

/**
 * @brief Recursively frees the memory allocated for all nodes in a subtree.
 * @param node The root of the subtree to free.
 */
void free_tree_nodes(BPlusTreeNode *node) {
    if (node == NULL) return;
    // Recursively free children if it's an internal node
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            free_tree_nodes(node->children[i]);
        }
    }
    // Free the node itself
    free(node);
}

/**
 * @brief Destroys the entire B+ Tree, freeing all allocated memory.
 * @param tree Pointer to the BPlusTree to destroy.
 */
void destroy_tree(BPlusTree *tree) {
    if (tree) {
        free_tree_nodes(tree->root); // Free all nodes starting from the root
        free(tree); // Free the tree structure itself
    }
}

// --- Utility (Example for Debugging) ---

/**
 * @brief Recursive helper function to print the tree structure.
 * @param node Current node being printed.
 * @param level Current depth level for indentation.
 */
void print_tree_recursive(BPlusTreeNode *node, int level) {
     if (node == NULL) return;

     // Indentation based on level
     for(int j=0; j<level; ++j) printf("  ");

     // Print node type and key count
     printf("[%p] %s Keys(%d): ", (void*)node, node->is_leaf ? "Leaf" : "Internal", node->num_keys);

     // Print keys (and offsets for leaves)
     for(int i=0; i < node->num_keys; i++) {
         printf("%d", node->keys[i]);
         if(node->is_leaf) printf("(%ld)", node->file_offsets[i]);
         printf(" ");
     }

     // Print parent and next pointers (useful for debugging)
     printf(" Parent: %p Next: %p\n", (void*)node->parent, (void*)node->next);

     // Recursively print children if it's an internal node
     if (!node->is_leaf) {
         for (int i = 0; i <= node->num_keys; i++) {
             print_tree_recursive(node->children[i], level + 1);
         }
     }
 }

/**
 * @brief Prints the structure of the B+ Tree and the linked list of leaves.
 * Useful for visualizing and debugging the tree.
 * @param tree Pointer to the BPlusTree.
 */
void print_tree(BPlusTree *tree) {
     if (!tree || !tree->root) {
         printf("Tree is empty.\n");
         return;
     }
     printf("\n--- B+ Tree Structure ---\n");
     print_tree_recursive(tree->root, 0);
     printf("--- End Tree Structure ---\n");

     // Optional: Print the linked list of leaf nodes
     printf("Leaf nodes linked list: ");
     BPlusTreeNode* current = tree->root;
     if (current) {
         // Find the first leaf node
         while(current && !current->is_leaf) {
             current = current->children[0];
         }
         // Traverse the linked list of leaves
         while(current) {
             printf("[");
             for(int i = 0; i < current->num_keys; ++i) {
                 printf("%d ", current->keys[i]);
             }
             printf("] -> ");
             current = current->next; // Move to the next leaf
         }
         printf("NULL\n");
     } else {
         printf("No leaf nodes found.\n");
     }
     printf("\n");
 }
