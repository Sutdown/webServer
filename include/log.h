#include "parse.h"

int logger_init();
int log_acc(Request *request, int response_code, int response_size);
int log_err(char *err_level, char *err_message);
char *get_client_ip();