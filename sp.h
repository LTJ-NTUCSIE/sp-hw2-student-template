#ifndef SP_H
#define SP_H

#include <stdio.h>

typedef struct {
  char *buf;
  size_t len;
  enum { CREATE, STORE, DELETE, FETCH, KILL, UPDATE, UNKNOWN } type;
} Command;

int command_handler(Command *cmd);
int create_handler(char *buf, size_t len);
int store_handler(char *buf, size_t len);
int delete_handler(char *buf, size_t len);
int fetch_handler(char *buf, size_t len);
int kill_handler(char *buf, size_t len);
int update_handler(char *buf, size_t len);

// handler stats :(
enum handler_status {
  H_ERR = -1,
  H_OK = 0,
  H_EXIT = 1
};


//-------------------------For input-----------------------------------
//------------------------------------------------------------------
#define READ_BUF_SIZE 4096
#define INIT_MSG_BUF_SIZE 32

typedef struct message{
  char read_buf[READ_BUF_SIZE];
  size_t read_len;
  char *not_processed_buf; // We need two bufs because of the max of message length is unsure :(
  size_t not_processed_len;
  size_t current_buf_capa;
} Message;

enum input_status {
    INPUT_ERROR = -1,
    INPUT_EOF = 0,
    INPUT_DATA = 1,
    INPUT_RETRY = 2
};

int init_msg(Message *m);
int cleanup_msg(Message *m);
int append_input(int fd, Message *m);
int classify_command(char *line, size_t len);
void cleanup_command(Command *c);
int pop_command(Message *m, Command *c);
int write_msg_to_fd(int fd, char *msg);
int pop_line(Message *m, char** res_line);
int shrink_current_length(Message* m);


//-----------------------For lookup table-----------------------------
//-------------------------------------------------------------------
typedef struct table_entry{
  char* key;
  char* val;
  size_t key_len;
  size_t val_len;
  struct table_entry* next;
} Table_entry;

// int init_table(Table_entry **table_start);
int new_table_entry(Table_entry **table_start, char* key, char* val);
int update_table_entry(Table_entry **table_start, char* key, char* val);
int kick_table_entry(Table_entry **table_start, char* key);
int search_in_table(Table_entry **table_start, char* key, char** if_val);
void cleanup_table(Table_entry **table_start);

#endif // SP_H