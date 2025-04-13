#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define MAX_WORD_LEN 100
#define MAX_LINE_LEN 1000
#define MAX_TRANSLATION_LEN 10000
#define TABLE_SIZE 20011 //  2x size of Spanish.txt lines 

typedef struct Entry {
    char key[MAX_WORD_LEN];
    char translation[MAX_TRANSLATION_LEN];
    int isDeleted;
} Entry;

typedef struct HashTable {
    Entry **table;
    int size;
    int itemCount;
    int totalProbes;
    int maxProbes;
    int probeHistogram[101]; // tracks 0–100 probes
    int userOps;
    int userProbes;
} HashTable;


unsigned long hash1(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

// second hash for double hashing
unsigned long hash2(const char *str) {
    unsigned long hash = 0;
    while (*str)
        hash = (hash * 131) + *str++;
    return (hash * 2 + 1); // ensure it's odd
}

HashTable* createTable(int size) {
    HashTable *ht = malloc(sizeof(HashTable));
    ht->table = calloc(size, sizeof(Entry*));
    ht->size = size;
    ht->itemCount = 0;
    ht->totalProbes = 0;
    ht->maxProbes = 0;
    ht->userOps = 0;
    ht->userProbes = 0;
    memset(ht->probeHistogram, 0, sizeof(ht->probeHistogram));
    return ht;
}

int insert(HashTable *ht, const char *key, const char *translation, int isFromFile) {
    unsigned long h1 = hash1(key) % ht->size;
    unsigned long h2 = hash2(key) % ht->size;
    int probes = 0;
    int index;

    for (int i = 0; i < ht->size; i++) {
        index = (h1 + i * h2) % ht->size;
        probes++;

        if (!ht->table[index] || ht->table[index]->isDeleted) {
            Entry *e = malloc(sizeof(Entry));
            strcpy(e->key, key);
            strcpy(e->translation, translation);
            e->isDeleted = 0;
            ht->table[index] = e;
            ht->itemCount++;
            break;
        } else if (strcmp(ht->table[index]->key, key) == 0) {
            // Duplicate: append
            strcat(ht->table[index]->translation, ";");
            strcat(ht->table[index]->translation, translation);
            break;
        }
    }

    if (isFromFile) {
        ht->totalProbes += probes;
        if (probes < 101) ht->probeHistogram[probes]++;
        if (probes > ht->maxProbes) ht->maxProbes = probes;
    } else {
        ht->userOps++;
        ht->userProbes += probes;
        printf("        %d probes\n", probes);
    }

    return probes;
}

Entry* search(HashTable *ht, const char *key, int *probesOut) {
    unsigned long h1 = hash1(key) % ht->size;
    unsigned long h2 = hash2(key) % ht->size;
    int probes = 0;
    int index;

    for (int i = 0; i < ht->size; i++) {
        index = (h1 + i * h2) % ht->size;
        probes++;

        if (!ht->table[index])
            break;
        if (!ht->table[index]->isDeleted && strcmp(ht->table[index]->key, key) == 0)
            break;
    }

    *probesOut = probes;
    ht->userOps++;
    ht->userProbes += probes;
    return ht->table[index] && !ht->table[index]->isDeleted && strcmp(ht->table[index]->key, key) == 0
           ? ht->table[index] : NULL;
}

void delete(HashTable *ht, const char *key) {
    int probes = 0;
    Entry *e = search(ht, key, &probes);

    printf("        %d probes\n", probes);
    if (e) {
        e->isDeleted = 1;
        printf("        Item was deleted.\n");
    } else {
        printf("        Item not found => no deletion.\n");
    }
}

void printStats(HashTable *ht) {
    printf("\nHash Table\n");
    printf("  average number of probes:               %.2f\n",
           ht->itemCount ? (double)ht->totalProbes / ht->itemCount : 0.0);
    printf("  max_run of probes:                      %d\n", ht->maxProbes);
    printf("  total PROBES (for %d items) :     %d\n", ht->itemCount, ht->totalProbes);
    printf("  items NOT hashed (out of %d):         0\n\n", ht->itemCount);

    printf("Probes|Count of keys\n-------------\n");
    for (int i = 1; i <= 100; i++) {
        if (ht->probeHistogram[i])
            printf("%6d|%6d\n-------------\n", i, ht->probeHistogram[i]);
    }
}

void toLower(char *s) {
    for (; *s; ++s) *s = tolower(*s);
}

int main() {
    char filename[51];
    char line[MAX_LINE_LEN];
    char word[MAX_WORD_LEN];
    char translation[MAX_TRANSLATION_LEN];

    printf("Enter the filename with the dictionary data (include the extension e.g. Spanish.txt):\n");
    scanf("%s", filename);
    getchar(); // consume newline

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Failed to open file.\n");
        return 1;
    }

    HashTable *ht = createTable(TABLE_SIZE);

    while (fgets(line, sizeof(line), fp)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;

        *tab = '\0';
        strcpy(word, line);
        strcpy(translation, tab + 1);
        translation[strcspn(translation, "\n")] = 0;
        toLower(word);

        insert(ht, word, translation, 1);
    }
    fclose(fp);
    printStats(ht);

    printf("\nEnter words to look-up. Enter q to stop.\n");

    while (fgets(line, sizeof(line), stdin)) {
        char op[3], w1[MAX_WORD_LEN], w2[MAX_TRANSLATION_LEN];
        int count = sscanf(line, "%s %s %s", op, w1, w2);
        toLower(w1);

        if (op[0] == 'q') break;

        printf("READ op:%s query:%s\n", op, w1);

        if (op[0] == 's') {
            int probes;
            Entry *e = search(ht, w1, &probes);
            printf("        %d probes\n", probes);
            if (e)
                printf("        Translation: %s\n", e->translation);
            else
                printf("        NOT found\n");

        } else if (op[0] == 'd') {
            delete(ht, w1);

        } else if (op[0] == 'i' && count == 3) {
            printf("        Will insert pair [%s,%s]\n", w1, w2);
            insert(ht, w1, w2, 0);
        }
    }

    printf("\nAverage probes per operation:     %.2f\n",
           ht->userOps ? (double)ht->userProbes / ht->userOps : 0.0);

    // Free all entries and the table itself
for (int i = 0; i < ht->size; i++) {
    if (ht->table[i]) {
        free(ht->table[i]);
    }
 }
 free(ht->table);
 free(ht);
   
return 0;
}
