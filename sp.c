#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

Table_entry *table_start;
int jud_rec_fd, jud_sen_fd;


int main(int argc, char *argv[]) {
  if (argc > 1) {
    fprintf(stderr, "Usage: ./sp\n");
    return 1;
  }

  // TODO: Determine whether the current process is SP main.
  //       After that, you may want to make the following code the same for both
  //       SP main and SP child processes. Therefore, you will set some
  //       variables here.

  // end TODO

  table_start = NULL;

  /* setup the channel for talking to judge */
  char *how_to_receive_from_judge, *how_to_send_to_judge;
  how_to_receive_from_judge = getenv("CMD_FIFO");
  how_to_send_to_judge = getenv("RLY_FIFO");

  if (how_to_receive_from_judge == NULL){
    jud_rec_fd = STDIN_FILENO;
  }
  else{
    jud_rec_fd = open(how_to_receive_from_judge, O_RDONLY);
  }
  if (how_to_send_to_judge == NULL){
    jud_sen_fd = STDOUT_FILENO;
  }
  else{
    jud_sen_fd = open(how_to_send_to_judge, O_WRONLY);
  }

  Message jud_kara_msg;
  if (init_msg(&jud_kara_msg) < 0){
    perror("calloc_died");
    return 1;
  }
  
  bool exiter = false;
  int input_status;
  /* Poll and pop commad section */ 
  while (1) {
    char *command_buffer = NULL;
    Command command = {.buf = command_buffer, .len = 0, .type = UNKNOWN};
    // TODO: poll the STDIO/FIFO for commands.
    struct pollfd poller = {.fd = jud_rec_fd, .events = POLLIN};
    int poll_result;
    if ((poll_result = poll(&poller, 1, -1)) < 0 ){
      if (errno == EINTR) {
        continue;
      }
      else{
        perror("Poll died");
        break;
      }
    }

    if (poller.revents & (POLLERR | POLLNVAL)){
      fprintf(stderr, "poll error\n");
      break;
    }
    else if ((poller.revents & (POLLIN | POLLHUP)) == 0){
      continue;
    }
    else{
      input_status = append_input(poller.fd, &jud_kara_msg);
      if (input_status == INPUT_ERROR){
        break;
      }
      if (input_status == INPUT_RETRY){
        continue;
      }
    }
    
    // end TODO

    // TODO: poll parent, and child processes for custom commands.

    // end TODO

    // TODO: classify the command and execute it.
    int pop_status;
    while ((pop_status=pop_command(&jud_kara_msg, &command)) > 0){
      int h_res = command_handler(&command);
      cleanup_command(&command);

      if (h_res == H_EXIT){
        exiter = true;
        break;
      }
      else if (h_res == H_ERR){
        fprintf(stderr, "Handler get err");
      }
    };

    if (exiter){
      break;
    }

    if (pop_status < 0){
      fprintf(stderr, "pop died\n");
      break;
    }
    
    if (input_status == INPUT_EOF){
      break;
    }
    // end TODO

    // TODO: handle custom commands.

    // end TODO
  }

  cleanup_msg(&jud_kara_msg);
  cleanup_table(&table_start);
  exit(0);
}

int command_handler(Command *cmd) {
  switch (cmd->type) {
  case CREATE:
    return create_handler(cmd->buf, cmd->len);
    break;
  case STORE:
    return store_handler(cmd->buf, cmd->len);
    break;
  case DELETE:
    return delete_handler(cmd->buf, cmd->len);
    break;
  case FETCH:
    return fetch_handler(cmd->buf, cmd->len);
    break;
  case KILL:
    return kill_handler(cmd->buf, cmd->len);
    break;
  case UPDATE:
    return update_handler(cmd->buf, cmd->len);
    break;
  default:
    fprintf(stderr, "Unknown command type\n");
    return -1;
  }
  return 0;
}

int create_handler(char *buf, size_t len) {
  // TODO: Implement the CREATE command handler.
  return 0;
}

int store_handler(char *buf, size_t len) {
  // TODO: Implement the STORE command handler.
  fprintf(stderr,"Not err but, STORE [%.*s]\n", (int)len, buf);
  if (len < 9){
    return -1;
  }
  size_t token_len = 6;

  while (token_len < len && buf[token_len] != ' ') {
    token_len++;
  }

  buf[token_len] = '\0';
  char *key = &(buf[6]); 
  char *val = &(buf[token_len+1]);
  return update_table_entry(&table_start, key, val);

  // return 0;
}

int delete_handler(char *buf, size_t len) {
  // TODO: Implement the DELETE command handler.
  if (len < 8){
    return -1;
  }

  return kick_table_entry(&(table_start), buf+7);
  // return 0;
}

// int direct_send_fetch_result(char *if_val){
//   size_t total = 0; 
//   size_t val_len = strlen(if_val);
//   while (total < val_len) {
//       ssize_t n = write(jud_sen_fd, if_val + total, val_len - total);
//       if (n > 0) {
//           total += (size_t)n;
//       } else if (n < 0 && errno == EINTR) {
//           continue;  // retry
//       } else {
//           return -1; // real err
//       }
//   }
//   return 0;
// }

int get_ans_from_consultant(char *key){
  char *is_root = getenv("CMD_FIFO");
  if (is_root == NULL){ //NULL means is root
    bool get_ok = false;
    Message c_res; char *line = NULL;
    while (! get_ok){
      int pc_pipe_fd[2], cp_pipe_fd[2]; //pc: parent write to child

      if (pipe(pc_pipe_fd)<0 || pipe(cp_pipe_fd)<0){
        perror("consult pipe died");
        return -1;
      }

      pid_t pid = fork();
      if (pid < 0){
        perror("consult fork died");
        return -1;
      }

      if (pid == 0){ // Consultant
        close(pc_pipe_fd[1]); close(cp_pipe_fd[0]); //fd[0] is read, [1] is write
        dup2(pc_pipe_fd[0], STDIN_FILENO); dup2(cp_pipe_fd[1], STDOUT_FILENO);
        close(pc_pipe_fd[0]); close(cp_pipe_fd[1]);

        char *consult = getenv("JUDGE_CONSULTANT");
        execl(consult,"consultant",NULL);

        perror("consult exec died");
        _exit(1);
        return -1;
      }
      else{ //SP main
        close(pc_pipe_fd[0]); close(cp_pipe_fd[1]);
        if ( write_msg_to_fd(pc_pipe_fd[1], key) < 0){
          close(pc_pipe_fd[1]); close(cp_pipe_fd[0]);
          return -1;
        }
        if ( write_msg_to_fd(pc_pipe_fd[1], "\n") < 0){
          close(pc_pipe_fd[1]); close(cp_pipe_fd[0]);
          return -1;
        }
        close(pc_pipe_fd[1]);

        
        init_msg(&c_res);
        while (true){
          int status = append_input(cp_pipe_fd[0], &c_res);
          if (status == INPUT_ERROR){
            break;
          }

          if (pop_line(&c_res, &line) == 1){
            if (strncmp(line, "Ok: ", 4) == 0){
              get_ok = true;
              break;
            }
            else{
              free(line);
              line = NULL;
              break;
            }
          }

          if (status == INPUT_EOF){
            break;
          }
        }
        cleanup_msg(&c_res);
        close(cp_pipe_fd[0]);
        waitpid(pid, NULL, 0);
      }
    }
    
    write_msg_to_fd(jud_sen_fd, line+4); //之後要改成給Target child
    write_msg_to_fd(jud_sen_fd, "\n");
    cleanup_msg(&c_res);
    if (line != NULL){
      free(line);
    }
  }
  else{ // not sp main, should ask parent

  }
}

int fetch_handler(char *buf, size_t len) {
  // TODO: Implement the FETCH command handler.
  fprintf(stderr, "Not err but, FETCH [%.*s]\n", (int)len, buf);
  size_t token_len = 6;
  while (token_len < len && buf[token_len] != ' ') {
    token_len++;
  }
  buf[token_len] = '\0';
  char *hash = &(buf[6]); 
  char *key = &(buf[token_len+1]);
  char *if_val; int success_search=-1;
  if ((success_search=search_in_table(&table_start,key,&if_val)) == 0 && if_val != NULL){
    write_msg_to_fd(jud_sen_fd, hash);
    write_msg_to_fd(jud_sen_fd, " ");
    write_msg_to_fd(jud_sen_fd, if_val);
    write_msg_to_fd(jud_sen_fd, "\n");
    return 0;
    // return direct_send_fetch_result(if_val);
  }
  else if (success_search == 0 && if_val == NULL){
    write_msg_to_fd(jud_sen_fd, hash);
    write_msg_to_fd(jud_sen_fd, " ");
    return get_ans_from_consultant(key);
  }
  else{
    return -1;
  }

  return -1;
}

int kill_handler(char *buf, size_t len) {
  // TODO: Implement the KILL command handler.
  (void) buf; (void) len;
  return H_EXIT;
}

int update_handler(char *buf, size_t len) {
  // TODO: Implement the UPDATE command handler.
  char* token = buf+7;
  struct stat st;
  if (lstat(token,&st) != 0){
    perror("lstat can't check the path");
    return -1;
  }
  if (!(S_ISREG(st.st_mode))){
    return -1;
  }
  if (access(token, X_OK) != 0){
    return -1;
  }

  if (setenv("JUDGE_CONSULTANT",token, 1) != 0){
    perror("setenv died");
    return -1;
  }
  return 0;
}
