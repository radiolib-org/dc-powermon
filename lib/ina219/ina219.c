#include "ina219.h"

#include <stdio.h>
#include <unistd.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <sys/ioctl.h>
#include <fcntl.h>

#define INA219_REG_CONFIG               (0x00)
#define INA219_REG_SHUNT_VOLTAGE        (0x01)
#define INA219_REG_BUS_VOLTAGE          (0x02)
#define INA219_REG_POWER                (0x03)
#define INA219_REG_CURRENT              (0x04)
#define INA219_REG_CALIBRATION          (0x05)

#define INA219_CFG_RESET                (0x01UL << 15)
#define INA219_CFG_WIDE_RANGE           (0x01UL << 13)
#define INA219_ADC_MODE_9BIT            (0x00UL << 3)
#define INA219_ADC_MODE_10BIT           (0x01UL << 3)
#define INA219_ADC_MODE_11BIT           (0x02UL << 3)
#define INA219_ADC_MODE_12BIT           (0x03UL << 3)
#define INA219_ADC_SAMPLES_2            (0x09UL << 3)
#define INA219_ADC_SAMPLES_4            (0x0AUL << 3)
#define INA219_ADC_SAMPLES_8            (0x0BUL << 3)
#define INA219_ADC_SAMPLES_16           (0x0CUL << 3)
#define INA219_ADC_SAMPLES_32           (0x0DUL << 3)
#define INA219_ADC_SAMPLES_64           (0x0EUL << 3)
#define INA219_ADC_SAMPLES_128          (0x0FUL << 3)

// file descriptor for accessing the I2C device
static int fd = -1;

static int ina219_read_register(uint8_t addr) {
  // write address to access
  if(write(fd, &addr, sizeof(addr)) != sizeof(addr)) {
    return(-1);
  }

  // read data from it
  uint8_t buff[2] = { 0 };
  if(read(fd, buff, sizeof(buff)) != sizeof(buff)) {
    return(-1);
  }

  return(buff[0] << 8 | buff[1]);
}

static int ina219_write_register(uint8_t addr, uint16_t val) {
  uint8_t buff[] = { addr, val >> 8, val & 0xff };
  if(write(fd, buff, sizeof(buff)) != sizeof(buff)) {
    return(-1);
  }
  return(0);
}

int ina219_begin(const char* i2c_path, int addr) {
  fd = open(i2c_path, O_RDWR);
  if(fd < 0) {
    return(-1);
  }

  int ret = ioctl(fd, I2C_SLAVE, addr);
  if(ret != 0)  {
    return(ret);
  }

  return(ina219_reset());
}

int ina219_end() {
  return(close(fd));
}

int ina219_reset() {
  return(ina219_write_register(INA219_REG_CONFIG, INA219_CFG_RESET));
}

void ina219_config_defaults(struct ina219_cfg_t* cfg) {
  if(!cfg) { return; }

  cfg->wide_range = true;
  cfg->pga = INA219_PGA_GAIN_DIV_8;
  cfg->bus_adc_mode_samples = INA219_ADC_MODE_12BIT;
  cfg->shunt_adc_mode_samples = INA219_ADC_MODE_12BIT;
  cfg->mode = INA219_MODE_SHUNT_AND_BUS_CONTINUOUS;
}

int ina219_config_set(struct ina219_cfg_t* cfg) {
  if(!cfg) { return(-1); }

  uint16_t val = 0 ;
  val |= cfg->wide_range ? INA219_CFG_WIDE_RANGE : 0;
  val |= cfg->pga << 11;
  val |= cfg->bus_adc_mode_samples << 4;
  val |= cfg->shunt_adc_mode_samples << 0;
  val |= cfg->mode;

  return(ina219_write_register(INA219_REG_CONFIG, val));
}

double ina219_read_shunt_voltage() {
  return((int16_t)ina219_read_register(INA219_REG_SHUNT_VOLTAGE) * 0.01);
}

double ina219_read_bus_voltage() {
  uint16_t raw = ina219_read_register(INA219_REG_BUS_VOLTAGE);
  return((int16_t)((raw >> 3) * 4) * 0.001);
}

int ina219_calibration_set(float max_current, int r_shunt) {

}
