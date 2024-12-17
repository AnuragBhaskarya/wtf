#ifndef HASH_TABLE_H
#define HASH_TABLE_H

// Typedef for the hash node structure
typedef struct HashNode {
    char *key;
    char *value;
    struct HashNode *next;
} HashNode;

// Typedef for the hash table structure
typedef struct {
    int size;
    HashNode **table;
} HashTable;

// New structure to hold multiple definitions
typedef struct {
    char **definitions;
    char **keys;  // New field to store original keys
    int count;
    int capacity;
} DefinitionList;




// Function prototypes
unsigned int hash_function(const char *key, int size);
HashTable* create_hash_table(int size);
void hash_table_insert(HashTable *table, const char *key, const char *value);
char* hash_table_lookup(HashTable *table, const char *key);
void free_hash_table(HashTable *table);

// New function prototypes
DefinitionList* hash_table_lookup_all(HashTable *table, const char *key);
void free_definition_list(DefinitionList *list);

#endif // HASH_TABLE_H