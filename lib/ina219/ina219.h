#ifndef INA219_H
#define INA219_H

#include <stdint.h>
#include <stdbool.h>

enum ina219_pga_gain_e {
  INA219_PGA_GAIN_1 = 0,
  INA219_PGA_GAIN_DIV_2,
  INA219_PGA_GAIN_DIV_4,
  INA219_PGA_GAIN_DIV_8,
};

enum ina219_mode_e {
  INA219_MODE_POWER_DOWN = 0,
  INA219_MODE_SHUNT_VOLTAGE_TRIGGERED,
  INA219_MODE_BUS_VOLTAGE_TRIGGERED,
  INA219_MODE_SHUNT_AND_BUS_TRIGGERED,
  INA219_MODE_ADC_OFF,
  INA219_MODE_SHUNT_VOLTAGE_CONTINUOUS,
  INA219_MODE_BUS_VOLTAGE_CONTINUOUS,
  INA219_MODE_SHUNT_AND_BUS_CONTINUOUS,
};

struct ina219_cfg_t {
  bool wide_range;
  enum ina219_pga_gain_e pga;
  uint8_t bus_adc_mode_samples;
  uint8_t shunt_adc_mode_samples;
  enum ina219_mode_e mode;
};

int ina219_begin(const char* i2c_path, int addr);
int ina219_end();
int ina219_reset();
void ina219_config_defaults(struct ina219_cfg_t* cfg);
int ina219_config_set(struct ina219_cfg_t* cfg);
double ina219_read_shunt_voltage();
double ina219_read_bus_voltage();

#endif
