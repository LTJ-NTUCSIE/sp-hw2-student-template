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
Outputer *jud_out_head; Outputer *jud_out_tail;
Cons_chat *chat_head; Cons_chat *chat_tail;
Message jud_kara_msg, jud_e_msg;
int jud_rec_fd, jud_sen_fd;
struct pollfd poller_for_all[70]; int poller_n;
int jud_sen_poller_id, cons_rec_poller_id, cons_sen_poller_id;
// struct pollfd judge_poller[2]; int judge_n;
// struct pollfd parent_poller[2]; int parent_n;
// struct pollfd children_poller[64]; int children_n;
// // struct pollfd consultant_poller[2]; int consultant_n;
// struct pollfd consultant_poller;

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
  jud_out_head = NULL;
  jud_out_tail = NULL;
  chat_head = NULL;
  chat_tail = NULL;

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

  if (init_msg(&jud_kara_msg) < 0) {
    return 1;
  }

  bool exiter = false;
  int input_status = INPUT_RETRY, output_status;
  poller_n = 1;
  poller_for_all[0].fd = jud_rec_fd; 
  poller_for_all[0].events = POLLIN;
  poller_for_all[0].revents = 0;
  // judge_n = 2;
  // judge_poller[0].fd = jud_rec_fd; judge_poller[0].events = POLLIN;
  // judge_poller[1].fd = jud_sen_fd; judge_poller[1].events = 0;
  /* Poll and pop commad section */ 
  while (1) {
    char *command_buffer = NULL;
    Command command = {.buf = command_buffer, .len = 0, .type = UNKNOWN};
    // TODO: poll the STDIO/FIFO for commands.
    // struct pollfd poller = {.fd = jud_rec_fd, .events = POLLIN};
    poller_n = 1;
    if (jud_out_head != NULL){
      poller_for_all[poller_n].fd = jud_sen_fd;
      poller_for_all[poller_n].events = POLLOUT;
      poller_for_all[poller_n].revents = 0;
      jud_sen_poller_id = poller_n;
      poller_n++;
    }
    if (chat_head != NULL){
      if (chat_head->state == SEND_KEY){
        poller_for_all[poller_n].fd = chat_head->send_fd;
        poller_for_all[poller_n].events = POLLOUT;
        poller_for_all[poller_n].revents = 0;
        cons_sen_poller_id = poller_n;
        poller_n++;
      }
      else if(chat_head->state == RECEIVING){
        poller_for_all[poller_n].fd = chat_head->recv_fd;
        poller_for_all[poller_n].events = POLLIN;
        poller_for_all[poller_n].revents = 0;
        cons_rec_poller_id = poller_n;
        poller_n++;
      }
    }
    int poll_result; int timeout = 100;
    if ((poll_result = poll(poller_for_all, poller_n, timeout)) < 0 ){
      if (errno == EINTR) {
        continue;
      }
      else{
        perror("Poll died");
        break;
      }
    }

    // Read command
    if (poller_for_all[0].revents & (POLLERR | POLLNVAL)){
      fprintf(stderr, "poll error\n");
      break;
    }
    else if ((poller_for_all[0].revents & (POLLIN | POLLHUP)) != 0){
      input_status = append_input(poller_for_all[0].fd, &jud_kara_msg);
      if (input_status == INPUT_ERROR){
        break;
      }
      if (input_status == INPUT_RETRY){
        continue;
      }
    }
    // Reply to judge
    if (jud_out_head != NULL){
      if (poller_for_all[jud_sen_poller_id].revents & (POLLERR | POLLNVAL)){
        fprintf(stderr, "poll error\n");
        break;
      }
      else if ((poller_for_all[jud_sen_poller_id].revents & (POLLOUT | POLLHUP)) !=0 ){
        if (jud_out_head != NULL){
          try_to_write(jud_out_head,jud_sen_fd);

          if (jud_out_head->processed_len == jud_out_head->total_len){
            complete_write(&jud_out_head, &jud_out_tail);
            if (jud_out_head == NULL){
              poller_for_all[jud_sen_poller_id].events = 0;
            }
          }
        }
      }
    }

    // end TODO

    if (chat_head != NULL){
      if (chat_head->state == SEND_KEY){
        if (poller_for_all[cons_sen_poller_id].revents & (POLLERR | POLLNVAL)){
          fprintf(stderr, "poll error\n");
          break;
        }
        else if ((poller_for_all[cons_sen_poller_id].revents & (POLLOUT | POLLHUP)) != 0){
          try_to_write(chat_head->send_first,chat_head->send_fd);

          if (chat_head->send_first->processed_len == chat_head->send_first->total_len){
            close(chat_head->send_fd);
            chat_head->send_fd = -1;
            chat_head->state = RECEIVING;
          }
        }
        
      }
      else if(chat_head->state == RECEIVING){
        if (poller_for_all[cons_rec_poller_id].revents & (POLLERR | POLLNVAL)){
          fprintf(stderr, "poll error\n");
          break;
        }
        else if ((poller_for_all[cons_rec_poller_id].revents & (POLLIN | POLLHUP)) != 0){
          int status = append_input(chat_head->recv_fd, chat_head->cons_res);
          if (status == INPUT_ERROR){
            chat_head->state = REAP;
          }

          if (pop_line(chat_head->cons_res, &(chat_head->cons_res_line)) == 1){
            chat_head->state = REAP;
          }

          if (status == INPUT_EOF){
            chat_head->state = REAP;
          }
        }        
      } 
      else{ //state == REAP
        int stat;
        pid_t result = waitpid(chat_head->cons_pid,&stat,WNOHANG);
        if (result < 0){
          perror("waitpid died");
          break;
        }

        else if (result == chat_head->cons_pid){
          char* key = NULL;
          char* hash = NULL;
          if (cons_chat_hantei(chat_head) == 0){
            key = strdup(chat_head->key);
            hash = strdup(chat_head->hash);
          }

          Cons_chat *temp = chat_head;
          chat_head = chat_head->next;
          if (chat_head == NULL){
            chat_tail = NULL;
          }
          end_a_cons_chat(temp);

          if (key!=NULL){
            start_a_cons_chat(hash, key);
            free(key);
            free(hash);
          }
        }
      }
    }

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

  if (chat_head!=NULL){
    end_a_cons_chat(chat_head);
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

int start_a_cons_chat(char *hash, char *key){
  int pc_pipe_fd[2], cp_pipe_fd[2]; //pc: parent write to child

  if (pipe(pc_pipe_fd)<0){
    perror("consult pipe died");
    return -1;
  }
  if (pipe(cp_pipe_fd)<0){
    close(pc_pipe_fd[0]); close(pc_pipe_fd[1]);
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
    fcntl(pc_pipe_fd[1], F_SETFD, FD_CLOEXEC);
    fcntl(cp_pipe_fd[0], F_SETFD, FD_CLOEXEC);
    

    Cons_chat *new_cons = (Cons_chat *)malloc(sizeof(Cons_chat));

    Message *new_res = (Message *)malloc(sizeof(Message));
    init_msg(new_res);

    Outputer *new_send = (Outputer *)malloc(sizeof(Outputer));
    new_send->total_len = strlen(key)+1; new_send->processed_len = 0; 
    new_send->next = NULL;
    new_send->buf = (char *)malloc(new_send->total_len);
    memcpy(new_send->buf,key,strlen(key));
    memcpy(new_send->buf+strlen(key),"\n",1);

    char *new_key = (char *)malloc(strlen(key)+1);
    memcpy(new_key, key, strlen(key));
    new_key[strlen(key)] = '\0';
    char *new_hash = (char *)malloc(strlen(hash)+1);
    memcpy(new_hash, hash, strlen(hash));
    new_hash[strlen(hash)] = '\0';

    int flags;
    new_cons->send_fd = pc_pipe_fd[1];
    new_cons->recv_fd = cp_pipe_fd[0];
    flags = fcntl(new_cons->send_fd, F_GETFL, 0);
    fcntl(new_cons->send_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(new_cons->recv_fd, F_GETFL, 0);
    fcntl(new_cons->recv_fd, F_SETFL, flags | O_NONBLOCK);
    new_cons->cons_pid = pid;
    new_cons->send_first = new_send;
    new_cons->key = new_key;
    new_cons->hash = new_hash;
    new_cons->cons_res = new_res;
    new_cons->cons_res_line = NULL;
    new_cons->state = SEND_KEY;
    new_cons->next = NULL;

    if (chat_head == NULL){
      chat_head = new_cons;
      chat_tail = new_cons;
    }
    else{
      chat_tail->next = new_cons;
      chat_tail = new_cons;
    }
    
    return 0;
    /*
    // if ( write_msg_to_fd(pc_pipe_fd[1], key) < 0){
    //   close(pc_pipe_fd[1]); close(cp_pipe_fd[0]);
    //   return -1;
    // }
    // if ( write_msg_to_fd(pc_pipe_fd[1], "\n") < 0){
    //   close(pc_pipe_fd[1]); close(cp_pipe_fd[0]);
    //   return -1;
    // }
    // close(pc_pipe_fd[1]);

    
    // init_msg(&c_res);
    // while (true){
    //   int status = append_input(cp_pipe_fd[0], &c_res);
    //   if (status == INPUT_ERROR){
    //     break;
    //   }

    //   if (pop_line(&c_res, &line) == 1){
    //     if (strncmp(line, "Ok: ", 4) == 0){
    //       get_ok = true;
    //       break;
    //     }
    //     else{
    //       free(line);
    //       line = NULL;
    //       break;
    //     }
    //   }

    //   if (status == INPUT_EOF){
    //     break;
    //   }
    // }
    // cleanup_msg(&c_res);
    // close(cp_pipe_fd[0]);
    // waitpid(pid, NULL, 0);
    */
  }
}

void end_a_cons_chat(Cons_chat *c){
  if (c == NULL){
    return ;
  }

  if (c->send_fd >= 0){
    close(c->send_fd);
  }
  if (c->recv_fd >= 0){
    close(c->recv_fd);
  }

  if (c->send_first != NULL){
    free(c->send_first->buf);
    free(c->send_first);
  }

  if (c->cons_res != NULL){
    cleanup_msg(c->cons_res);
    free(c->cons_res);
  }

  free(c->cons_res_line);
  free(c->key);
  free(c->hash);

  free(c);
  return;
}

int cons_chat_hantei(Cons_chat *c){
  if (c->cons_res_line == NULL){
    return 0;
  }

  if (strncmp(c->cons_res_line, "Ok: ", 4) == 0){
    add_fetch_outputer(&jud_out_head, &jud_out_tail, c->hash, (c->cons_res_line)+4);
    return 1;
  }
  else{
    // start_a_cons_chat(chat_head->hash, chat_head->key);
    return 0;
  }
}

int get_ans_from_ue(char *hash, char *key){
  char *is_root = getenv("CMD_FIFO");
  if (is_root == NULL){ //NULL means is root
    return start_a_cons_chat(hash, key);
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
    return add_fetch_outputer(&jud_out_head, &jud_out_tail, hash, if_val);
    // return direct_send_fetch_result(if_val);
  }
  else if (success_search == 0 && if_val == NULL){
    return get_ans_from_ue(hash,key);
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
