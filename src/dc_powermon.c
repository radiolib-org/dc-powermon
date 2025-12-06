#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "argtable3/argtable3.h"
#include "ina219/ina219.h"
#include "socket/socket.h"
#include "dc-powermon-client/dc_powermon_cmds.h"

#ifndef GITREV
#define GITREV "unknown"
#endif

#define STR_HELPER(s) #s    
#define STR(s) STR_HELPER(s)     

// some default configuration values
#define INA219_ADDR_DEFAULT       0x40  // default for unmodified RadioHAT Rev. C
#define WINDOW_DEFAULT            128
#define CONTROL_DEFAULT           41123

// buffer for commands from socket
static char socket_buff[256] = { 0 };

static struct conf_t {
  int window;
  int socket_fd;
} conf = {
  .window = WINDOW_DEFAULT,
  .socket_fd = -1,
};

enum sample_type_e {
  V_BUS = 0,
  V_SHUNT,
  I_SHUNT,
  P_SHUNT,
  NUM_SAMPLE_TYPES,
};

// structure to save data about a single sample
// TODO add timestamp
struct sample_t {
  double val[NUM_SAMPLE_TYPES];
};

// structure holding information about the minimum and maximum
static struct stats_t {
  struct sample_t min;
  struct sample_t max;
  struct sample_t avg;
} stats = {
  .min = { .val = {  99,  99,  9999,  9999 } },
  .max = { .val = { -99, -99, -9999, -9999 } },
  .avg = { .val = {   0,   0,     0,     0 } },
};

// averaging window
#define BUFF_SIZE             4096
static struct sample_t avg_window[BUFF_SIZE] = { 0 };
static struct sample_t* avg_ptr = avg_window;

// argtable arguments
static struct args_t {
  struct arg_int* addr;
  struct arg_dbl* max_current;
  struct arg_dbl* r_shunt;
  struct arg_int* window;
  struct arg_int* control;
  struct arg_lit* help;
  struct arg_end* end;
} args;

static void sighandler(int signal) {
  (void)signal;
  exit(EXIT_SUCCESS);
}

static void exithandler(void) {
  fprintf(stdout, "\n");
  fflush(stdout);
  int ret = ina219_end();
  if(ret < 0) {
    fprintf(stderr, "ERROR: Failed to close I2C port\n");
  }
}

static void stats_reset() {
  stats.min.val[V_BUS] = 99; stats.min.val[V_SHUNT] = 99;
  stats.min.val[I_SHUNT] = 9999; stats.min.val[P_SHUNT] = 9999;
  stats.max.val[V_BUS] = -99; stats.max.val[V_SHUNT] = -99;
  stats.max.val[I_SHUNT] = -9999; stats.max.val[P_SHUNT] = -9999;
  stats.avg.val[V_BUS] = 0; stats.avg.val[V_SHUNT] = 0;
  stats.avg.val[I_SHUNT] = 0; stats.avg.val[P_SHUNT] = 0;
}

static void stats_update(struct sample_t* sample) {
  memcpy(avg_ptr, sample, sizeof(struct sample_t));

  for(int i = 0; i < NUM_SAMPLE_TYPES; i++) {
    // update statistics
    if(sample->val[i] < stats.min.val[i]) {
      stats.min.val[i] = sample->val[i];
    } else if(sample->val[i] > stats.max.val[i]) {
      stats.max.val[i] = stats.min.val[i];
    }
    
    // calculate the average
    stats.avg.val[i] = 0;
    for(int j = 0; j < conf.window; j++) {
      stats.avg.val[i] += (double)avg_window[j].val[i];
    }
    stats.avg.val[i] /= conf.window;
  }

  avg_ptr++;
  if((avg_ptr - avg_window) > conf.window) {
    avg_ptr = avg_window;
  }
}

static void process_socket_cmd(int fd, char* cmd) {
  char buff[64] = { 0 };
  if(strstr(cmd, DC_POWERMON_CMD_READ_POWER) == cmd) {
    sprintf(buff, "%.2fmW" DC_POWERMON_RSP_LINEFEED, (double)stats.avg.val[P_SHUNT]);
  
  } else if(strstr(cmd, DC_POWERMON_CMD_READ_CURRENT) == cmd) {
    sprintf(buff, "%.2fmA" DC_POWERMON_RSP_LINEFEED, (double)stats.avg.val[I_SHUNT]);
  
  } else if(strstr(cmd, DC_POWERMON_CMD_READ_V_BUS) == cmd) {
    sprintf(buff, "%.2fV" DC_POWERMON_RSP_LINEFEED, (double)stats.avg.val[V_BUS]);
  
  } else if(strstr(cmd, DC_POWERMON_CMD_READ_V_SHUNT) == cmd) {
    sprintf(buff, "%.2fmV" DC_POWERMON_RSP_LINEFEED, (double)stats.avg.val[V_SHUNT]);

  } else if(strstr(cmd, DC_POWERMON_CMD_RESET) == cmd) {
    stats_reset();
    sprintf(buff, DC_POWERMON_RSP_LINEFEED);
  
  } else if(strstr(cmd, DC_POWERMON_CMD_ID) == cmd) {
    sprintf(buff, "radiolib-org,DCpowerMon," GITREV DC_POWERMON_RSP_LINEFEED);

  } else if(strstr(cmd, DC_POWERMON_CMD_SYSTEM_EXIT) == cmd) {
    raise(SIGINT);

  } else {
    // TODO handle invalid commands?
    fprintf(stderr, "invalid socket cmd: %s\n", cmd);

  }

  socket_write(fd, buff);
}

static int run() {
  // start readout
  fprintf(stdout, "   V_bus     V_shunt    I_shunt     P_shunt\n");
  struct sample_t sample;
  int read_socket_fd = 0;
  for(;;) {
    sample.val[V_BUS] = ina219_read_bus_voltage();
    sample.val[V_SHUNT] = ina219_read_shunt_voltage();
    sample.val[I_SHUNT] = ina219_read_current();
    sample.val[P_SHUNT] = sample.val[I_SHUNT] * sample.val[V_BUS]; // it is a lot faster to multiply than send it over the I2C bus

    // update statistics
    stats_update(&sample);

    fprintf(stdout, " %6.2f V  %6.2f mV %7.2f mA  %7.2f mW\r", stats.avg.val[V_BUS], stats.avg.val[V_SHUNT], stats.avg.val[I_SHUNT], stats.avg.val[P_SHUNT]);
    fflush(stdout);

    // check if there is something to read from the socket
    read_socket_fd = socket_read(conf.socket_fd, socket_buff);
    if(read_socket_fd > 0) {
      // got something, process it
      process_socket_cmd(read_socket_fd, socket_buff);

      // close the socket now, we're done with it
      close(read_socket_fd);
    }
  }

  return(0);
}

int main(int argc, char** argv) {
  void *argtable[] = {
    args.addr = arg_int0("a", "addr", NULL, "I2C address of the INA219, defaults to " STR(INA219_ADDR_DEFAULT)),
    args.max_current = arg_dbl0("i", "max_current", "Amps", "Maximum current expected to flow through the shunt resistor, defaults to 1.0 A"),
    args.r_shunt = arg_dbl0("r", "r_shunt", "milliOhms", "Shunt resistor value, defaults to 100.0 mOhm"),
    args.window = arg_int0("w", "window", NULL, "Averaging window length, defaults to " STR(WINDOW_DEFAULT)),
    args.control = arg_int0("c", "control", "port", "Control port for socket connection, defaults to " STR(CONTROL_DEFAULT)),
    args.help = arg_lit0(NULL, "help", "Display this help and exit"),
    args.end = arg_end(2),
  };

  int exitcode = 0;
  if(arg_nullcheck(argtable) != 0) {
    fprintf(stderr, "%s: insufficient memory\n", argv[0]);
    exitcode = 1;
    goto exit;
  }

  int nerrors = arg_parse(argc, argv, argtable);
  if(args.help->count > 0) {
    fprintf(stdout, "INA219 power monitor, gitrev " GITREV "\n");
    fprintf(stdout, "Usage: %s", argv[0]);
    arg_print_syntax(stdout, argtable, "\n");
    fprintf(stdout, "After start, send SIGINT /Ctrl+C/ to stop\n");
    arg_print_glossary(stdout, argtable,"  %-25s %s\n");
    exitcode = 0;
    goto exit;
  }

  if(nerrors > 0) {
    arg_print_errors(stdout, args.end, argv[0]);
    fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
    exitcode = 1;
    goto exit;
  }

  atexit(exithandler);
  signal(SIGINT, sighandler);

  // parse arguments
  int addr = INA219_ADDR_DEFAULT;
  if(args.addr->count) { addr = args.addr->ival[0]; }
  double max_current = 1.0;
  if(args.max_current->count) { max_current = args.max_current->dval[0]; }
  double r_shunt = 100.0;
  if(args.r_shunt->count) { max_current = args.r_shunt->dval[0]; }

  // set up the socket
  int socket_port = CONTROL_DEFAULT;
  if(args.control->count) { socket_port = args.control->ival[0]; };
  conf.socket_fd = socket_setup(socket_port);

  // start the power meter
  int ret = ina219_begin("/dev/i2c-1", addr);
  if(ret) {
    fprintf(stderr, "ERROR: Failed to open I2C port\n");
    return(ret);
  }

  // set the configuration and calibration
  struct ina219_cfg_t ina_cfg;
  ina219_config_defaults(&ina_cfg);
  ina_cfg.wide_range = false;
  ina219_calibration_set(max_current, r_shunt);
  ina219_config_set(&ina_cfg);

  exitcode = run();

exit:
  arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));

  return exitcode;
}
