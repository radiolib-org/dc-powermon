#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "argtable3/argtable3.h"
#include "ina219/ina219.h"

#ifndef GITREV
#define GITREV "unknown"
#endif

#define STR_HELPER(s) #s    
#define STR(s) STR_HELPER(s)     

// some default configuration values
#define INA219_ADDR_DEFAULT       0x40  // default for unmodified RadioHAT Rev. C

// argtable arguments
static struct args_t {
  struct arg_int* addr;
  struct arg_dbl* max_current;
  struct arg_dbl* r_shunt;
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

static int run() {
  // start readout
  fprintf(stdout, "   V_bus     V_shunt    I_shunt     P_shunt\n");
  double v_bus, v_shunt, i_shunt, p_shunt;
  for(;;) {
    v_bus = ina219_read_bus_voltage();
    v_shunt = ina219_read_shunt_voltage();
    i_shunt = ina219_read_current();
    p_shunt = i_shunt * v_bus; // it is a lot faster to multiply than send it over the I2C bus
    fprintf(stdout, " %6.2f V  %6.2f mV %7.2f mA  %7.2f mW\r", v_bus, v_shunt, i_shunt, p_shunt);
    fflush(stdout);
  }

  return(0);
}

int main(int argc, char** argv) {
  void *argtable[] = {
    args.addr = arg_int0("a", "addr", NULL, "I2C address of the INA219, defaults to " STR(INA219_ADDR_DEFAULT)),
    args.max_current = arg_dbl0("i", "max_current", "Amps", "Maximum current expected to flow through the shunt ressistor, defaults to 1.0 A"),
    args.r_shunt = arg_dbl0("r", "r_shunt", "milliOhms", "Shunt resistor value, defaults to 100.0 mOhm"),
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
