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

#define READ_BUF_SIZE 4096

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
int append_input(int fd, Message *m);
int classify_command(char *line, size_t len);
void cleanup_command(Command *c);
int pop_command(Message *m, Command *c);

#endif // SP_H