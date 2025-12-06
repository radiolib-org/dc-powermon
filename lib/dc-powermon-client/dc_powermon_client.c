#include "dc_powermon_client.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <netdb.h>

#include "dc_powermon_cmds.h"

static struct sockaddr_in server = {
  .sin_family = AF_INET,
  .sin_port = 0,
  .sin_addr = { 0 },
  .sin_zero = { 0 },
};

typedef int (*cb_setup_t)(void);
typedef int (*cb_read_t)(int, char*);
typedef void (*cb_write_t)(int, const char*);

static cb_setup_t cb_setup = NULL;
static cb_read_t cb_read = NULL;
static cb_write_t cb_write = NULL;

static int socket_setup(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) {
    return(-1);
  }

  int ret = connect(fd, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
  if(ret) {
    return(-1);
  }

  return(fd);
}

static int socket_read(int socket, char* cmd_buff) {
  int len = 0;
  do {
    ioctl(socket, FIONREAD, &len);
  } while(!len);

  len = recv(socket, cmd_buff, len, 0);
  if(len < 0) {
    // something went wrong
    return(0);
  }
  cmd_buff[len] = '\0';

  // strip trailing newline if present
  if((strlen(cmd_buff) > 1) && (cmd_buff[strlen(cmd_buff) - 1] == '\n')) {
    cmd_buff[strlen(cmd_buff) - 1] = '\0';
  }

  return(len);
}

static void socket_write(int socket, const char* data) {
  (void)send(socket, data, strlen(data), 0);
}

static int scpi_exec(const char* cmd, char* rpl_buff) {
  if(!cb_read || !cb_write || !cb_setup) {
    return(EXIT_FAILURE);
  }

  int fd = cb_setup();
  if(fd < 0) {
    return(EXIT_FAILURE);
  }
  
  cb_write(fd, cmd);
  if(rpl_buff) {
    (void)cb_read(fd, rpl_buff);
  }
  (void)close(fd);
  return(EXIT_SUCCESS);
}

int dc_powermon_init_socket(const char* hostname, int port) {
  struct hostent* hostnm = gethostbyname(hostname);
  if(!hostnm) {
    return(EXIT_FAILURE);
  }

  memcpy(&server.sin_addr, hostnm->h_addr_list[0], hostnm->h_length);
  server.sin_port = htons(port);
  cb_read = socket_read;
  cb_write = socket_write;
  cb_setup = socket_setup;
  return(EXIT_SUCCESS);
}

int dc_powermon_read_power(float* val) {
  char rpl_buff[256];
  int ret = scpi_exec(DC_POWERMON_CMD_READ_POWER, rpl_buff);
  if(val) { *val = strtof(rpl_buff, NULL); }
  return(ret);
}

int dc_powermon_exit() {
  return(scpi_exec(DC_POWERMON_CMD_SYSTEM_EXIT, NULL));
}

int dc_powermon_reset() {
  return(scpi_exec(DC_POWERMON_CMD_RESET, NULL));
}

int dc_powermon_id(char* buff) {
  return(scpi_exec(DC_POWERMON_CMD_ID, buff));
}
