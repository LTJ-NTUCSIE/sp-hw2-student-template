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


bool is_sp_main;
Table_entry *table_start;

int jud_rec_fd, jud_sen_fd;
Message jud_kara_msg;
Outputer *jud_out_head; Outputer *jud_out_tail;
int jud_sen_poller_id, jud_rec_poller_id;

int par_rec_fd, par_sen_fd; //Child: stdio; SP main:NULL
Message par_kara_msg;
Outputer *par_out_head; Outputer *par_out_tail;
int par_sen_poller_id, par_rec_poller_id;

Child *child_head;

Cons_chat *chat_head; //Cons_chat *chat_tail;
// int cons_rec_poller_id, cons_sen_poller_id; // Maybe switch to use the one in cons struct.

Lookupreq *lookup_head;

struct pollfd *poller_for_all; int poller_n; int poller_capa;
// struct pollfd poller_for_all[128]; int poller_n;
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

  is_sp_main = (getenv("CMD_FIFO") == NULL);

  table_start = NULL;
  jud_out_head = NULL;
  jud_out_tail = NULL;
  chat_head = NULL;
  // chat_tail = NULL;
  par_out_head = NULL;
  par_out_tail = NULL;

  /* setup the channel for talking to judge */
  char *how_to_receive_from_judge, *how_to_send_to_judge;
  how_to_receive_from_judge = getenv("CMD_FIFO");
  how_to_send_to_judge = getenv("RLY_FIFO");

  if (is_sp_main){
    jud_rec_fd = STDIN_FILENO;
    // fcntl(jud_rec_fd, F_SETFD, FD_CLOEXEC);

    jud_sen_fd = STDOUT_FILENO;
    // fcntl(jud_sen_fd, F_SETFD, FD_CLOEXEC);
  }
  else{
    par_rec_fd = STDIN_FILENO;
    par_sen_fd = STDOUT_FILENO;
    // fcntl(par_rec_fd, F_SETFD, FD_CLOEXEC);
    // fcntl(par_sen_fd, F_SETFD, FD_CLOEXEC);

    mkfifo(how_to_receive_from_judge, 0666);
    mkfifo(how_to_send_to_judge, 0666);

    jud_rec_fd = open(how_to_receive_from_judge, O_RDONLY);
    fcntl(jud_rec_fd, F_SETFD, FD_CLOEXEC);
    unlink(how_to_receive_from_judge);

    jud_sen_fd = open(how_to_send_to_judge, O_WRONLY);
    fcntl(jud_sen_fd, F_SETFD, FD_CLOEXEC);
    unlink(how_to_send_to_judge);
  }
  
  int flags;
  flags = fcntl(jud_rec_fd, F_GETFL, 0);
  fcntl(jud_rec_fd, F_SETFL, flags | O_NONBLOCK);

  flags = fcntl(jud_sen_fd, F_GETFL, 0);
  fcntl(jud_sen_fd, F_SETFL, flags | O_NONBLOCK);

  if (init_msg(&jud_kara_msg) < 0) {
    return 1;
  }
  if (!is_sp_main && init_msg(&par_kara_msg) < 0) {
    return 1;
  }

  bool exiter = false;
  int input_status = INPUT_RETRY, output_status;
  
  poller_capa = 64;
  init_poller(&poller_for_all, &poller_n, &poller_capa);
  // judge_n = 2;
  // judge_poller[0].fd = jud_rec_fd; judge_poller[0].events = POLLIN;
  // judge_poller[1].fd = jud_sen_fd; judge_poller[1].events = 0;
  /* Poll and pop commad section */ 
  while (1) {
    char *command_buffer = NULL;
    Command command = {.buf = command_buffer, .len = 0, .type = UNKNOWN};
    // TODO: poll the STDIO/FIFO for commands.
    // struct pollfd poller = {.fd = jud_rec_fd, .events = POLLIN};
    poller_n = 0;
    if (true){ //read judge
      if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
        exiter = true;
        break;
      }
      poller_for_all[poller_n].fd = jud_rec_fd; 
      poller_for_all[poller_n].events = POLLIN;
      poller_for_all[poller_n].revents = 0;
      jud_rec_poller_id = poller_n;
      poller_n++;
    }
    if (jud_out_head != NULL){
      if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
        exiter = true;
        break;
      }
      poller_for_all[poller_n].fd = jud_sen_fd;
      poller_for_all[poller_n].events = POLLOUT;
      poller_for_all[poller_n].revents = 0;
      jud_sen_poller_id = poller_n;
      poller_n++;
    }
    if (!is_sp_main){
      if (true){
        if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
          exiter = true;
          break;
        }
        poller_for_all[poller_n].fd = par_rec_fd;
        poller_for_all[poller_n].events = POLLIN;
        poller_for_all[poller_n].revents = 0;
        par_rec_poller_id = poller_n;
        poller_n++;
      }
      if (par_out_head != NULL){
        if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
          exiter = true;
          break;
        }
        poller_for_all[poller_n].fd = par_sen_fd;
        poller_for_all[poller_n].events = POLLOUT;
        poller_for_all[poller_n].revents = 0;
        par_sen_poller_id = poller_n;
        poller_n++;
      }
    }
    
    Child* child_walker = child_head;
    while (child_walker != NULL){
      if (child_walker->state == DIED){
        child_walker = child_walker->next;
        continue;
      }
      if (true){
        if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
          exiter = true;
          break;
        }
        poller_for_all[poller_n].fd = child_walker->from_child_fd;
        poller_for_all[poller_n].events = POLLIN;
        poller_for_all[poller_n].revents = 0;
        child_walker->rec_poller_id = poller_n;
        poller_n++;
      }
      if (child_walker->out_to_child_head != NULL){
        if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
          exiter = true;
          break;
        }
        poller_for_all[poller_n].fd = child_walker->to_child_fd;
        poller_for_all[poller_n].events = POLLOUT;
        poller_for_all[poller_n].revents = 0;
        child_walker->sen_poller_id = poller_n;
        poller_n++;
      } 

      child_walker = child_walker->next;
    }

    Cons_chat *chat_walker = chat_head;
    while (chat_walker != NULL){
      if (chat_walker->state == SEND_KEY){
        if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
          exiter = true;
          break;
        }
        poller_for_all[poller_n].fd = chat_walker->send_fd;
        poller_for_all[poller_n].events = POLLOUT;
        poller_for_all[poller_n].revents = 0;
        chat_walker->sen_poller_id = poller_n;
        poller_n++;
      }
      else if(chat_walker->state == RECEIVING){
        if (ensure_poller_is_enough(&poller_for_all, &poller_n, &poller_capa) < 0){
          exiter = true;
          break;
        }
        poller_for_all[poller_n].fd = chat_walker->recv_fd;
        poller_for_all[poller_n].events = POLLIN;
        poller_for_all[poller_n].revents = 0;
        chat_walker->rec_poller_id = poller_n;
        poller_n++;
      }

      chat_walker = chat_walker->next;
    }

    // Check Every FD
    int poll_result; int timeout = 10;
    if ((poll_result = poll(poller_for_all, poller_n, timeout)) < 0 ){
      if (errno == EINTR) {
        cleanup_poller(&poller_for_all, &poller_n, &poller_capa);
        continue;
      }
      else{
        perror("Poll died");
        break;
      }
    }

    // Read command
    if (poller_for_all[jud_rec_poller_id].revents & (POLLERR | POLLNVAL)){
      fprintf(stderr, "poll error\n");
      break;
    }
    else if ((poller_for_all[jud_rec_poller_id].revents & (POLLIN | POLLHUP)) != 0){
      input_status = append_input(poller_for_all[jud_rec_poller_id].fd, &jud_kara_msg);
      if (input_status == INPUT_ERROR){
        break;
      }
      // if (input_status == INPUT_RETRY){
      //   continue;
      // }
    }
    // Reply to judge
    if (jud_out_head != NULL){
      if (poller_for_all[jud_sen_poller_id].revents & (POLLERR | POLLNVAL)){
        fprintf(stderr, "poll error\n");
        break;
      }
      else if ((poller_for_all[jud_sen_poller_id].revents & (POLLOUT | POLLHUP)) !=0 ){
        try_to_write(jud_out_head,jud_sen_fd);

        if (jud_out_head->processed_len == jud_out_head->total_len){
          complete_write(&jud_out_head, &jud_out_tail);
          if (jud_out_head == NULL){
            poller_for_all[jud_sen_poller_id].events = 0;
          }
        }
      }
    }

    if (!is_sp_main){
      // receive from parent
      if (poller_for_all[par_rec_poller_id].revents & (POLLERR | POLLNVAL)){
        fprintf(stderr, "poll error\n");
        break;
      }
      else if ((poller_for_all[par_rec_poller_id].revents & (POLLIN | POLLHUP)) != 0){
        int parent_input_status;
        parent_input_status = append_input(poller_for_all[par_rec_poller_id].fd, &par_kara_msg);
        if (parent_input_status == INPUT_ERROR){
          break;
        }
        else if (parent_input_status == INPUT_EOF){
          break;
        }
        else{
          char* parent_res = NULL;
          while (pop_line(&par_kara_msg, &parent_res) > 0){
            size_t cutter = 0;
            size_t par_res_len = strlen(parent_res);
            while (cutter < par_res_len && parent_res[cutter] != ' ') {
              cutter++;
            }
            parent_res[cutter] = '\0';
            if (strlen(parent_res) == 0 || strlen(parent_res+cutter+1) == 0){
              free(parent_res);
              continue;
            }
            fetch_thing_to_where_hantei(parent_res, parent_res+cutter+1);
            free(parent_res);
          }
        }
        
      }
      // Reply to parent
      if (par_out_head != NULL){
        if (poller_for_all[par_sen_poller_id].revents & (POLLERR | POLLNVAL)){
          fprintf(stderr, "poll error\n");
          break;
        }
        else if ((poller_for_all[par_sen_poller_id].revents & (POLLOUT | POLLHUP)) !=0 ){
          try_to_write(par_out_head, par_sen_fd);

          if (par_out_head->processed_len == par_out_head->total_len){
            complete_write(&par_out_head, &par_out_tail);
            if (par_out_head == NULL){
              poller_for_all[par_sen_poller_id].events = 0;
            }
          }
        }
      }
    }

    child_walker = child_head;
    while (child_walker != NULL){
      if (child_walker->state == DIED){
        child_walker = child_walker->next;
        continue;
      }
      if (poller_for_all[child_walker->rec_poller_id].revents & (POLLERR | POLLNVAL)){
        exiter = true;
        fprintf(stderr, "poll error\n");
        break;
      }
      else if ((poller_for_all[child_walker->rec_poller_id].revents & (POLLIN | POLLHUP)) != 0){
        int child_input_status;
        child_input_status = append_input(poller_for_all[child_walker->rec_poller_id].fd, child_walker->msg_from_child);
        if (child_input_status == INPUT_ERROR){
          close(child_walker->from_child_fd);
          child_walker->from_child_fd = -1;
          close(child_walker->to_child_fd);
          child_walker->to_child_fd = -1;
          child_walker->state = DIED;
          // break;
        }
        else if (child_input_status == INPUT_EOF){
          close(child_walker->from_child_fd);
          child_walker->from_child_fd = -1;
          close(child_walker->to_child_fd);
          child_walker->to_child_fd = -1;
          child_walker->state = DIED;
          // break;
        }
        else{
          char* child_req = NULL;
          while (pop_line(child_walker->msg_from_child, &child_req) > 0){
            size_t cutter = 0;
            size_t chi_res_len = strlen(child_req);
            while (cutter < chi_res_len && child_req[cutter] != ' ') {
              cutter++;
            }
            child_req[cutter] = '\0';
            if (strlen(child_req) == 0 || strlen(child_req+cutter+1) == 0){
              free(child_req);
              continue;
            }
            check_a_child_req(child_req, child_req+cutter+1, child_walker);
            free(child_req);
          }
        }
      }

      if (child_walker->out_to_child_head != NULL){
        if (poller_for_all[child_walker->sen_poller_id].revents & (POLLERR | POLLNVAL)){
          fprintf(stderr, "poll error\n");
          break;
        }
        else if ((poller_for_all[child_walker->sen_poller_id].revents & (POLLOUT | POLLHUP)) !=0 ){
          try_to_write(child_walker->out_to_child_head, child_walker->to_child_fd);

          if (child_walker->out_to_child_head->processed_len == child_walker->out_to_child_head->total_len){
            complete_write(&child_walker->out_to_child_head, &child_walker->out_to_child_tail);
            if (child_walker->out_to_child_head == NULL){
              poller_for_all[child_walker->sen_poller_id].events = 0;
            }
          }
        }
      }
      
      child_walker = child_walker->next;
    }

    // end TODO

    chat_walker = chat_head;
    while (chat_walker != NULL){
      if (chat_walker->state == SEND_KEY){
        if (poller_for_all[chat_walker->sen_poller_id].revents & (POLLERR | POLLNVAL)){
          fprintf(stderr, "poll error\n");
          break;
        }
        else if ((poller_for_all[chat_walker->sen_poller_id].revents & (POLLOUT | POLLHUP)) != 0){
          try_to_write(chat_walker->send_first,chat_walker->send_fd);

          if (chat_walker->send_first->processed_len == chat_walker->send_first->total_len){
            close(chat_walker->send_fd);
            chat_walker->send_fd = -1;
            chat_walker->state = RECEIVING;
          }
        }
        
      }
      else if(chat_walker->state == RECEIVING){
        if (poller_for_all[chat_walker->rec_poller_id].revents & (POLLERR | POLLNVAL)){
          fprintf(stderr, "poll error\n");
          break;
        }
        else if ((poller_for_all[chat_walker->rec_poller_id].revents & (POLLIN | POLLHUP)) != 0){
          int cons_input_status = append_input(chat_walker->recv_fd, chat_walker->cons_res);
          if (cons_input_status == INPUT_ERROR){
            // close(chat_walker->recv_fd);
            // chat_walker->recv_fd = -1;
            chat_walker->state = REAP;
          }

          if (pop_line(chat_walker->cons_res, &(chat_walker->cons_res_line)) == 1){
            // close(chat_walker->recv_fd);
            // chat_walker->recv_fd = -1;
            chat_walker->state = REAP;
          }

          if (cons_input_status == INPUT_EOF){
            // close(chat_walker->recv_fd);
            // chat_walker->recv_fd = -1;
            chat_walker->state = REAP;
          }
        }        
      } 
      // else{ //state == REAP
      //   int stat;
      //   pid_t result = waitpid(chat_walker->cons_pid,&stat,WNOHANG);
      //   if (result < 0){
      //     perror("waitpid died when consult");
      //     break;
      //   }

      //   else if (result == chat_walker->cons_pid){
      //     char* key = NULL;
      //     char* hash = NULL;
      //     if (cons_chat_hantei(chat_walker) == 0){
      //       key = strdup(chat_walker->key);
      //       hash = strdup(chat_walker->hash);
      //     }

      //     Cons_chat *temp = chat_head;
      //     chat_head = chat_head->next;
      //     end_a_cons_chat(temp);

      //     if (key!=NULL){
      //       start_a_cons_chat(hash, key);
      //       free(key);
      //       free(hash);
      //     }
      //   }
      // }

      chat_walker = chat_walker->next;
    }

    // TODO: poll parent, and child processes for custom commands.

    // end TODO

    // TODO: classify the command and execute it.
    int pop_status;
    while ((pop_status=pop_command(&jud_kara_msg, &command)) > 0){
      // fprintf(stderr, "%s\n", command);
      int h_res = command_handler(&command);
      cleanup_command(&command);

      if (h_res == H_EXIT){
        exiter = true;
        break;
      }
      else if (h_res == H_ERR){
        fprintf(stderr, "Handler get err");
        exiter = true;
        break;
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

    int forker_status;
    pid_t who_died; bool finish_the_loop = true;
    while ((who_died = waitpid(-1,&forker_status, WNOHANG)) > 0){
      finish_the_loop = true;
      child_walker = child_head;
      while (child_walker != NULL){
        if (child_walker->pid == who_died){
          wipe_out_a_child(child_walker);
          finish_the_loop = false;
          break;
        }
        child_walker = child_walker->next;
      }
      if (!finish_the_loop){
        continue;
      }
      chat_walker = chat_head;
      while (chat_walker != NULL){
        if (chat_walker->cons_pid == who_died){
          if (cons_chat_hantei(chat_walker) == 0){
            start_a_cons_chat(chat_walker->hash, chat_walker->key);
          }
          end_a_cons_chat(chat_walker);
          break;
        }
        chat_walker = chat_walker->next;
      }
    };

    // cleanup_poller(&poller_for_all,&poller_n,&poller_capa);    
  }
  fprintf(stderr,"breh");
  cleanup_everything();
  // while (chat_head!=NULL){
  //   Cons_chat *temp = chat_head;
  //   chat_head = chat_head->next;
  //   end_a_cons_chat(temp);
  // }
  // cleanup_msg(&jud_kara_msg);
  // cleanup_table(&table_start);
  exit(0);
}

int init_poller(struct pollfd **poller, int *poller_n, int* poller_capa){
  *poller_capa = 64;
  *poller = (struct pollfd *)malloc(sizeof(struct pollfd) * (*poller_capa));
  *poller_n = 0;
  if (*poller == NULL){
    *poller_capa = 0;
    return -1;
  }

  return 0;
}

void cleanup_poller(struct pollfd **poller, int *poller_n, int* poller_capa){
  free(*poller);
  *poller_n = 0;
  *poller_capa = 0;
}

int expand_poller(struct pollfd **poller, int* poller_capa){
  int new_size = (*poller_capa == 0 ? 64 : (*poller_capa)*2);
  struct pollfd *new_p = realloc(*poller, sizeof(struct pollfd)*new_size);
  if (new_p == NULL){
    return -1;
  }
  *poller_capa = new_size;
  *poller = new_p;
  return 0;
}

int ensure_poller_is_enough(struct pollfd **poller, int *poller_n, int* poller_capa){
  while (*poller_n >= *poller_capa ){
    int status = expand_poller(poller,poller_capa);
    if (status < 0){
      return -1;
    }
  }
  return 0;
}

void cleanup_everything(){
  Child *child_walker = child_head;
  while (child_walker != NULL){
    if (child_walker->to_child_fd != -1){
      close(child_walker->to_child_fd);
      child_walker->to_child_fd = -1;
    }
    if (child_walker->from_child_fd != -1){
      close(child_walker->from_child_fd);
      child_walker->from_child_fd = -1;
    }

    child_walker = child_walker->next;
  }
  Cons_chat *chat_walker = chat_head;
  while (chat_walker != NULL){
    if (chat_walker->recv_fd != -1){
      close(chat_walker->recv_fd);
      chat_walker->recv_fd = -1;
    }
    if (chat_walker->send_fd != -1){
      close(chat_walker->send_fd);
      chat_walker->send_fd = -1;
    }

    chat_walker = chat_walker->next;
  }

  while (true){
    int forker_status;
    pid_t who_died = waitpid(-1,&forker_status, 0);
    if (who_died == -1 && errno == ECHILD){
      break;
    }
    child_walker = child_head;
    while (child_walker != NULL){
      if (child_walker->pid == who_died){
        wipe_out_a_child(child_walker);
        break;
      }
      child_walker = child_walker->next;
    }
    chat_walker = chat_head;
    while (chat_walker != NULL){
      if (chat_walker->cons_pid == who_died){
        end_a_cons_chat(chat_walker);
        break;
      }
      chat_walker = chat_walker->next;
    }
  }

  cleanup_msg(&jud_kara_msg);
  while (jud_out_head != NULL){
    Outputer *temp = jud_out_head;
    jud_out_head = jud_out_head->next;
    cleanup_outputer(temp);
  }
  if (!is_sp_main){
    cleanup_msg(&par_kara_msg);
    while (par_out_head != NULL){
      Outputer *temp = par_out_head;
      par_out_head = par_out_head->next;
      cleanup_outputer(temp);
    }
  }

  cleanup_table(&table_start);
  // cleanup_lookup(lookup_head);
  while (lookup_head != NULL){
    Lookupreq *temp = lookup_head;
    lookup_head = lookup_head->next;
    free(temp->hash);
    free(temp);
  }

  cleanup_poller(&poller_for_all, &poller_n, &poller_capa);
  return;
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

int summon_new_child(char *cmd, char *rly){
  int pc_pipe_fd[2], cp_pipe_fd[2]; //pc: parent write to child

  if (pipe(pc_pipe_fd)<0){
    perror("child pipe died");
    return -1;
  }
  if (pipe(cp_pipe_fd)<0){
    close(pc_pipe_fd[0]); close(pc_pipe_fd[1]);

    perror("child pipe died");
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0){
    perror("consult fork died");
    close(pc_pipe_fd[0]); close(cp_pipe_fd[1]);
    close(pc_pipe_fd[1]); close(cp_pipe_fd[0]);
    return -1;
  }

  if (pid == 0){ // Child
    close(pc_pipe_fd[1]); close(cp_pipe_fd[0]); //fd[0] is read, [1] is write
    dup2(pc_pipe_fd[0], STDIN_FILENO); dup2(cp_pipe_fd[1], STDOUT_FILENO);
    close(pc_pipe_fd[0]); close(cp_pipe_fd[1]);

    // int cmd_fifo_len = strlen(cmd) + 9;
    // int rly_fifo_len = strlen(rly) + 9;
    // char *cmd_fifo = (char *)malloc(cmd_fifo_len +1);
    // char *rly_fifo = (char *)malloc(rly_fifo_len +1);
    // snprintf(cmd_fifo, cmd_fifo_len, "CMD_FIFO=%s", cmd);
    // snprintf(rly_fifo, rly_fifo_len, "RLY_FIFO=%s", rly);
    // char *envp[] = {cmd_fifo, rly_fifo, NULL};
    // char *argv[] = {"./sp", NULL};
    // execve("./sp", argv, envp);

    setenv("CMD_FIFO", cmd, 1);
    setenv("RLY_FIFO", rly, 1);
    execl("./sp", "./sp", (char *)NULL);


    perror("Child exec died");
    // free(cmd_fifo); free(rly_fifo);

    _exit(1);
    return -1;
  }
  else{ //Parent
    close(pc_pipe_fd[0]); close(cp_pipe_fd[1]);
    fcntl(pc_pipe_fd[1], F_SETFD, FD_CLOEXEC);
    fcntl(cp_pipe_fd[0], F_SETFD, FD_CLOEXEC);
    
    Child *new_child = (Child *)malloc(sizeof(Child));
    new_child->pid = pid;
    new_child->to_child_fd = pc_pipe_fd[1];
    new_child->from_child_fd = cp_pipe_fd[0];

    Message *new_res = (Message *)malloc(sizeof(Message));
    init_msg(new_res);
    new_child->msg_from_child = new_res;

    int flags;
    flags = fcntl(new_child->to_child_fd, F_GETFL, 0);
    fcntl(new_child->to_child_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(new_child->from_child_fd, F_GETFL, 0);
    fcntl(new_child->from_child_fd, F_SETFL, flags | O_NONBLOCK);
    new_child->out_to_child_head = NULL;
    new_child->out_to_child_tail = NULL;
    
    new_child->rec_poller_id = -1;
    new_child->sen_poller_id = -1;
    new_child->state = OKAY;

    new_child->next = child_head;

    child_head = new_child;    
  }

  return 0;
}

int wipe_out_a_child(Child *c){
  if (c == NULL){
    return 0;
  }

  if (c == child_head){
    child_head = c->next;
  }
  else{
    Child *pre = child_head;
    Child *child_walker = child_head->next;
    while (child_walker != NULL){
      if (child_walker == c){
        pre->next = child_walker->next;
        break;
      }
      pre = pre->next;
      child_walker = child_walker->next;
    }
  }

  if (c->to_child_fd != -1){
    close(c->to_child_fd);
  }
  if (c->from_child_fd != -1){
    close(c->from_child_fd);
  }
  cleanup_msg(c->msg_from_child);
  free(c->msg_from_child);
  while (c->out_to_child_head != NULL){
    Outputer *temp2 = c->out_to_child_head;
    c->out_to_child_head = c->out_to_child_head->next;
    cleanup_outputer(temp2);
  }
  free(c);
  return 0;
}

int create_handler(char *buf, size_t len) {
  // TODO: Implement the CREATE command handler.
  fprintf(stderr,"Not err but, STORE [%.*s]\n", (int)len, buf);
  if (len < 10){
    return -1;
  }
  size_t token_len = 7;

  while (token_len < len && buf[token_len] != ' ') {
    token_len++;
  }

  buf[token_len] = '\0';
  char *cmd_fifo = &(buf[7]); 
  char *rly_fifo = &(buf[token_len+1]);
  return summon_new_child(cmd_fifo,rly_fifo);
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
    close(pc_pipe_fd[0]); close(pc_pipe_fd[1]);
    close(cp_pipe_fd[1]); close(cp_pipe_fd[0]);
    perror("consult fork died");
    return -1;
  }

  if (pid == 0){ // Consultant
    close(pc_pipe_fd[1]); close(cp_pipe_fd[0]); //fd[0] is read, [1] is write
    dup2(pc_pipe_fd[0], STDIN_FILENO); dup2(cp_pipe_fd[1], STDOUT_FILENO);
    close(pc_pipe_fd[0]); close(cp_pipe_fd[1]);

    char *consult = getenv("JUDGE_CONSULTANT");
    if (consult == NULL){
      fprintf(stderr,"Why getenv died");
      exit(1);
      return -1;
    }
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
    new_cons->rec_poller_id = -1; 
    new_cons->sen_poller_id = -1;
    new_cons->cons_pid = pid;
    new_cons->send_first = new_send;
    new_cons->key = new_key;
    new_cons->hash = new_hash;
    new_cons->cons_res = new_res;
    new_cons->cons_res_line = NULL;
    new_cons->state = SEND_KEY;
    new_cons->next = chat_head;

    chat_head = new_cons;
    
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
    return;
  }

  if (c == chat_head){
    chat_head = chat_head->next;
  }
  else if (chat_head != NULL){
    Cons_chat *pre = chat_head;
    Cons_chat *chat_walker = chat_head->next;
    while (chat_walker != NULL){
      if (chat_walker == c){
        pre->next = c->next;
        break;
      }

      pre = pre->next;
      chat_walker = chat_walker->next;
    }
  }

  if (c->send_fd >= 0){
    close(c->send_fd);
    c->send_fd = -1;
  }
  if (c->recv_fd >= 0){
    close(c->recv_fd);
    c->recv_fd = -1;
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
    // Lookupreq *lookup_walker = lookup_head;
    // while(lookup_walker != NULL){
    //   if (strcmp(c->hash, lookup_walker->hash) == 0){
    //     if (lookup_walker->target_child == NULL){
    //       add_fetch_outputer(&jud_out_head, &jud_out_tail, c->hash, (c->cons_res_line)+4);
    //     }
    //     else{
    //       add_fetch_outputer(&(lookup_walker->target_child->out_to_child_head), 
    //       &(lookup_walker->target_child->out_to_child_tail), c->hash, (c->cons_res_line)+4);
    //     }
    //     break;
    //   }
    //   lookup_walker= lookup_walker->next;
    // }
    (void) fetch_thing_to_where_hantei(c->hash, (c->cons_res_line)+4);

    return 1;
  }
  else{
    // start_a_cons_chat(chat_head->hash, chat_head->key);
    return 0;
  }
}

int fetch_thing_to_where_hantei(char *hash, char *val){
  if (lookup_head == NULL){
    return 0;
  }

  if (strcmp(hash, lookup_head->hash) == 0){
    if (lookup_head->target_child == NULL){
      add_fetch_outputer(&jud_out_head, &jud_out_tail, hash, val);
    }
    else{
      add_fetch_outputer(&(lookup_head->target_child->out_to_child_head), 
      &(lookup_head->target_child->out_to_child_tail), hash, val);
    }
    Lookupreq *temp = lookup_head;
    lookup_head = lookup_head->next;
    free(temp->hash);
    free(temp);


    return 0;
  }

  Lookupreq *pre = lookup_head;
  Lookupreq *lookup_walker = lookup_head->next;
  while(lookup_walker != NULL){
    if (strcmp(hash, lookup_walker->hash) == 0){
      if (lookup_walker->target_child == NULL){
        add_fetch_outputer(&jud_out_head, &jud_out_tail, hash, val);
      }
      else{
        add_fetch_outputer(&(lookup_walker->target_child->out_to_child_head), 
        &(lookup_walker->target_child->out_to_child_tail), hash, val);
      }
      pre->next = lookup_walker->next;
      free(lookup_walker->hash);
      free(lookup_walker);
      break;
    }
    pre = pre->next;
    lookup_walker= lookup_walker->next;
  }

  return 0;
}

int make_new_request(char *hash, Child *req_child){
  Lookupreq *new_lookup = (Lookupreq *)malloc(sizeof(Lookupreq));

  char *new_hash = (char *)malloc(strlen(hash)+1);
  memcpy(new_hash, hash, strlen(hash));
  new_hash[strlen(hash)] = '\0';

  new_lookup->hash = new_hash;
  new_lookup->target_child = req_child;
  new_lookup->next = lookup_head;
  lookup_head = new_lookup;

  return 0;
}

int get_ans_from_ue(char *hash, char *key, Child* req_child){
  if (is_sp_main){
    (void) make_new_request(hash,req_child);
    return start_a_cons_chat(hash, key);
  }
  else{ // not sp main, should ask parent
    (void) make_new_request(hash,req_child);
    return add_fetch_outputer(&par_out_head, &par_out_tail, hash, key);
  }
}

int check_a_child_req(char *hash, char *key, Child* req_child){
  char *if_val = NULL;
  int success_search = search_in_table(&table_start, key, &(if_val));
  if (success_search == 0 && if_val!=NULL){
    return add_fetch_outputer(&(req_child->out_to_child_head), &(req_child->out_to_child_tail), hash, if_val);
  }
  else{
    return get_ans_from_ue(hash, key, req_child);
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
    return get_ans_from_ue(hash,key,NULL);
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
