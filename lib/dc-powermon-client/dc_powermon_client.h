#ifndef DC_POWERMON_CLIENT_H
#define DC_POWERMON_CLIENT_H

#ifdef __cplusplus
extern "C"{
#endif 

int dc_powermon_init_socket(const char* hostname, int port);
int dc_powermon_read_power(float* val);
int dc_powermon_exit();
int dc_powermon_reset();
int dc_powermon_id(char* buff);

#ifdef __cplusplus
}
#endif

#endif
