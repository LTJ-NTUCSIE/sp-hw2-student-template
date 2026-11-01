#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int new_table_entry(Table_entry **table_start, char* key, char* val){
    size_t key_len = strlen(key), val_len = strlen(val);
    
    char* key_line = (char*) malloc (sizeof(char) * (key_len+1)); 
    if (key_line == NULL){
        return -1;
    }
    strncpy(key_line,key,key_len); key_line[key_len] = '\0';

    char* val_line = (char*) malloc (sizeof(char) * (val_len+1)); 
    if (val_line == NULL){
        free(key_line);
        return -1;
    }
    strncpy(val_line,val,val_len); val_line[val_len] = '\0';
    
    Table_entry *new_entry = (Table_entry*) malloc (sizeof(Table_entry));
    if (new_entry == NULL){
        free(key_line);
        free(val_line);
        return -1;
    }
    new_entry->key = key_line; new_entry->key_len = key_len;
    new_entry->val = val_line; new_entry->val_len = val_len;
    new_entry->next = (*table_start);
    (*table_start) = new_entry;    

    return 0;
}

int update_table_entry(Table_entry **table_start, char* key, char* val){
    Table_entry *walker = (*table_start);
    while (walker != NULL){
        if (walker->key != NULL && strcmp(walker->key,key) == 0){
            size_t val_len = strlen(val);
            char* val_line = (char*) malloc (sizeof(char) * (val_len+1)); 
            if (val_line == NULL){
                return -1;
            }
            strncpy(val_line,val,val_len); val_line[val_len] = '\0';
            
            char *temp = walker->val;
            walker->val = val_line; walker->val_len = val_len;
            free(temp);
            return 0;
        }
        walker=walker->next;
    } 
    return new_table_entry(table_start, key, val);   
}

int kick_table_entry(Table_entry **table_start, char* key){
    if (*table_start == NULL){
        return 0;
    }

    if (strcmp((*table_start)->key,key) == 0){
        free((*table_start)->key);
        free((*table_start)->val);
        Table_entry *temp = (*table_start);
        (*table_start) = (*table_start)->next;
        free(temp);
        return 0;
    }
    else{
        Table_entry *walker = (*table_start);
        while (walker->next != NULL){
            if (walker->next->key != NULL && strcmp(walker->next->key,key) == 0){
                Table_entry *temp = walker->next;
                walker->next = walker->next->next;
                free(temp->key);
                free(temp->val);
                free(temp);
                
                return 0;
            }
            walker=walker->next;
        } 
    }   

    return 0;
}

int search_in_table(Table_entry **table_start, char* key, char** if_val){
    Table_entry *walker = (*table_start);
    *if_val = NULL;
    while (walker != NULL){
        if (walker->key != NULL && strcmp(walker->key,key) == 0){
            // char* val_line = (char*) malloc (sizeof(char) * (walker->val_len+1)); 
            // if (val_line == NULL){
            //     fprintf(stderr, "mallocing val dies\n");
            //     return -1;
            // }
            // strncpy(val_line,walker->val,walker->val_len); val_line[walker->val_len] = '\0';
            
            // (* if_val) = val_line;

            // (*if_val) = walker->val;

            //second
            // size_t total = 0; 
            // char *p = walker->val;
            // while (total < walker->val_len) {
            //     ssize_t n = write(STDOUT_FILENO, p + total, walker->val_len - total);

            //     if (n > 0) {
            //         total += (size_t)n;
            //     } else if (n < 0 && errno == EINTR) {
            //         continue;  // retry
            //     } else {
            //         return -1; // real err
            //     }
            // }
            
            (*if_val) = walker->val;
            return 0;
        }
        walker=walker->next;
    } 
    return 0;
}

void cleanup_table(Table_entry **table_start){
    while ((*table_start) != NULL){
        Table_entry *temp = (*table_start);
        (*table_start) =(*table_start)->next;
        free(temp->key);
        free(temp->val);
        free(temp);
    }
}

// int init_table(Table_entry **table_start){
//     (*table_start)->key = NULL;
//     (*table_start)->val = NULL;
//     (*table_start)->next = NULL;
//     return 1;
// }