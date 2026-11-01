#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


int init_msg(Message *m){
  m->read_len = 0;
  m->not_processed_len = 0;
  m->current_buf_capa = INIT_MSG_BUF_SIZE;
  if ((m->not_processed_buf = calloc(INIT_MSG_BUF_SIZE, sizeof(char))) == NULL){
    return -1;
  }
  return 0;
}

int cleanup_msg(Message *m){
  free(m->not_processed_buf);
  m->not_processed_buf = NULL;
  m->read_len = 0;
  m->not_processed_len = 0;
  m->current_buf_capa = 0;
  return 0;
}

int check_if_current_len_is_enough(Message* m){
  size_t nec_size = m->not_processed_len + m->read_len + 1;

  if (nec_size > m->current_buf_capa){
    size_t new_capa = m->current_buf_capa;
    while(new_capa < nec_size){
      new_capa = new_capa*2;
    }

    char *new_buf;
    if ((new_buf = realloc(m->not_processed_buf, new_capa)) == NULL){
      perror("increase realloc died");
      return -1;
    }

    m->not_processed_buf = new_buf;
    m->current_buf_capa = new_capa;
    return 0;
  }

  return 0;
} 

int shrink_current_length(Message* m){
  size_t nec_size = m->not_processed_len + 1;

  if (m->current_buf_capa > INIT_MSG_BUF_SIZE && m->current_buf_capa > 2*nec_size){
    size_t new_capa = m->current_buf_capa;
    while (new_capa > INIT_MSG_BUF_SIZE && new_capa > 2*nec_size){
      new_capa = new_capa/2;
    }

    char *new_buf = realloc(m->not_processed_buf, new_capa);
    if (new_buf == NULL){
      perror("shrink realloc died");
      return -1;
    }

    m->not_processed_buf = new_buf;
    m->current_buf_capa = new_capa;
    return 0;
  }
  
  return 0;
}

void add_read_to_proc_buf(Message *m){
  memcpy(m->not_processed_buf + m->not_processed_len, m->read_buf, m->read_len);
  m->not_processed_len = m->not_processed_len + m->read_len;
  return;
}

int append_input(int fd, Message* m) {
  ssize_t read_status;
  if ( (read_status=read(fd, m->read_buf, sizeof(m->read_buf))) < 0){
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK){
      return INPUT_RETRY;
    }

    perror("Read died");
    return INPUT_ERROR;
  }
  else if (read_status == 0){ //EOF
    m -> read_len = 0;
    return INPUT_EOF; 
  }

  m->read_len = (size_t) read_status;
  if (check_if_current_len_is_enough(m) < 0){
    
    return INPUT_ERROR;
  }
  add_read_to_proc_buf(m);
  
  // if ()


  // m->read_len += (size_t)read_status;
  return INPUT_DATA;
}

int classify_command(char* line, size_t len){
  size_t token_len = 0;

  while (token_len < len && line[token_len] != ' ' && line[token_len] != '\t') {
    token_len++;
  }

  if (token_len == 6 && strncmp(line, "create", token_len) == 0) {
    return CREATE;
  }

  if (token_len == 5 && strncmp(line, "store", token_len) == 0) {
    return STORE;
  }

  if (token_len == 6 && strncmp(line, "delete", token_len) == 0) {
    return DELETE;
  }

  if (token_len == 5 && strncmp(line, "fetch", token_len) == 0) {
    return FETCH;
  }

  if (token_len == 4 && strncmp(line, "kill", token_len) == 0) {
    return KILL;
  }

  if (token_len == 6 && strncmp(line, "update", token_len) == 0) {
    return UPDATE;
  }

  return UNKNOWN;
}

void cleanup_command(Command *c){
  free(c->buf);
  c->len = 0;
  c->type = UNKNOWN;
}

int pop_command(Message *m, Command *c) {
  char *line = NULL;
  int pop_status;
  if ((pop_status=pop_line(m,&line)) < 0){
    return -1;
  }
  else if (pop_status == 0){
    return 0;
  }
  else{
    c->buf = line; c->len = strlen(line); c->type = classify_command(c->buf, c->len);
    return 1;
  }

  return 0;
    // char *newline = memchr(m->not_processed_buf, '\n', m->not_processed_len);
    // if (newline == NULL) {
    //     return 0;
    // }
    // size_t consumed = (size_t)(newline - m->not_processed_buf) + 1;
    // size_t len = consumed - 1;
    // if (len > 0 && m->not_processed_buf[len - 1] == '\r') {
    //     --len;
    // }
    // // if (len + 1 > out_size) {
    // //     return -1;
    // // }

    // char *line;
    // if ((line= malloc(len+1)) == NULL){
    //   perror("Malloc died");
    //   return -1;
    // }

    // memcpy(line, m->not_processed_buf, len);
    // line[len] = '\0';
    // memmove(m->not_processed_buf, m->not_processed_buf + consumed, m->not_processed_len - consumed);
    // m->not_processed_len -= consumed;
    // shrink_current_length(m);
    // c->buf = line; c->len = len; c->type=classify_command(line,len);
    // return 1;
}

int write_msg_to_fd(int fd, char *msg){
  size_t msg_len = strlen(msg);
  size_t total = 0;
  while (total < msg_len) {
    ssize_t n = write(fd, msg + total, msg_len - total);
    if (n > 0) {
        total += (size_t)n;
    } else if (n < 0 && errno == EINTR) {
        continue;  // retry
    } else {
        return -1; // real err
    }
  }
  return 0;
}

int pop_line(Message *m, char **res_line){
  char *newline = memchr(m->not_processed_buf, '\n', m->not_processed_len);
  if (newline == NULL) {
      return 0;
  }
  size_t consumed = (size_t)(newline - m->not_processed_buf) + 1;
  size_t len = consumed - 1;
  if (len > 0 && m->not_processed_buf[len - 1] == '\r') {
      --len;
  }
  // if (len + 1 > out_size) {
  //     return -1;
  // }

  char *line;
  if ((line= malloc(len+1)) == NULL){
    perror("Malloc died");
    return -1;
  }

  memcpy(line, m->not_processed_buf, len);
  line[len] = '\0';
  memmove(m->not_processed_buf, m->not_processed_buf + consumed, m->not_processed_len - consumed);
  m->not_processed_len -= consumed;
  shrink_current_length(m);
  *res_line = line;
  return 1;
}