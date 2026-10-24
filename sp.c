#include "sp.h"

#include <stdio.h>
#include <stdlib.h>

#include <poll.h>
#include <errno.h>
#include <unistd.h>

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

  Message stdin_msg;
  if (init_msg(&stdin_msg) < 0){
    perror("calloc_died");
    return 1;
  }
  
  while (1) {
    char *command_buffer = NULL;
    Command command = {.buf = command_buffer, .len = 0, .type = UNKNOWN};
    // TODO: poll the STDIO/FIFO for commands.
    struct pollfd poller = {.fd = STDIN_FILENO, .events = POLLIN};
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
      fprintf(stderr, "poll error");
      break;
    }
    else if ((poller.revents & (POLLIN | POLLHUP)) == 0){
      continue;
    }
    else{
      int input_status;
      if ((input_status=append_input(poller.fd, &stdin_msg)) == INPUT_ERROR){
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
    while ((pop_status=pop_command(&stdin_msg, &command)) > 0){
      command_handler(&command);
      cleanup_command(&command);
    };

    if (pop_status < 0){
      fprintf(stderr, "pop died\n");
      break;
    }
    
    // end TODO

    // TODO: handle custom commands.

    // end TODO
  }
}

int command_handler(Command *cmd) {
  switch (cmd->type) {
  case CREATE:
    create_handler(cmd->buf, cmd->len);
    break;
  case STORE:
    store_handler(cmd->buf, cmd->len);
    break;
  case DELETE:
    delete_handler(cmd->buf, cmd->len);
    break;
  case FETCH:
    fetch_handler(cmd->buf, cmd->len);
    break;
  case KILL:
    kill_handler(cmd->buf, cmd->len);
    break;
  case UPDATE:
    update_handler(cmd->buf, cmd->len);
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
  printf("STORE [%.*s]\n", (int)len, buf);
  return 0;
}

int delete_handler(char *buf, size_t len) {
  // TODO: Implement the DELETE command handler.
  return 0;
}

int fetch_handler(char *buf, size_t len) {
  // TODO: Implement the FETCH command handler.
  printf("FETCH [%.*s]\n", (int)len, buf);
  return 0;
}

int kill_handler(char *buf, size_t len) {
  // TODO: Implement the KILL command handler.
  return 0;
}

int update_handler(char *buf, size_t len) {
  // TODO: Implement the UPDATE command handler.
  return 0;
}
