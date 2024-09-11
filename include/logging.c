#include <stdio.h>
#include "logging.h"

void log_variable(const char* var_name, int value, const char* file, int line) {
    FILE* log_file = fopen("/home/lynniepro21/working/trace_state/real_transitions.log", "a");
    if (log_file != NULL) {
        fprintf(log_file, "%s:%d: %s = %d\n", file, line, var_name, value);
        fclose(log_file);
    } else {
        fprintf(stderr, "Error opening log file.\n");
    }
}


