enum {
    SWITCH_NO_ERROR = 0,
    SWITCH_OPEN_TTY_FAIL = -1
} SWITCH_ERROR_CODES;


#define SWITCH_ERROR_STR(__err)  switch_err_msgs[-1*__err]
// This is defined in switch.c
extern const char * switch_err_msgs[NUM_SWITCH_ERRORS];

int switch_init();
double switch_get_state();
