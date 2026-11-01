#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int add_fetch_outputer(Outputer **head, Outputer **tail, char *hash, char* val){
  Outputer *new_outer = (Outputer *) malloc(sizeof(Outputer));
  if (new_outer == NULL){
    return -1;
  }
  size_t hash_len = strlen(hash);
  size_t val_len = strlen(val);
  new_outer->total_len = hash_len+val_len+2;
  new_outer->processed_len = 0;
  new_outer->next = NULL;
  new_outer->buf = (char *)malloc(new_outer -> total_len);
  memcpy(new_outer->buf, hash, hash_len);
  memcpy(new_outer->buf+hash_len, " ", 1);
  memcpy(new_outer->buf+hash_len+1, val, val_len);
  memcpy(new_outer->buf+hash_len+1+val_len, "\n", 1);
  
  if (*tail == NULL){
    (*head) = new_outer;
    (*tail) = new_outer;
  }
  else{
    (*tail)->next = new_outer;
    (*tail) = new_outer;
  }

  return 0;
}

int try_to_write(Outputer *o, int fd){
  ssize_t n = write(fd, o->buf+o->processed_len, o->total_len - o->processed_len);
  if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)){
    return 0;
  }
  else if (n > 0){
    o->processed_len += n;
    return 0;
  }
  return -1;
}

int complete_write(Outputer **s, Outputer **e){
  if (*s == NULL){
    return 0;
  }
  Outputer *temp = *s;
  *s = (*s)->next;
  if (*s == NULL){
    *e = NULL;
  }
  free(temp->buf);
  free(temp);
  return 0;
}