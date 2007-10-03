#include <stddef.h>

struct model_symbols {
    char *key;
    char *file_name;
    unsigned int len;
    char *data;
};

extern const int model_symbols_cnt;
extern const struct model_symbols model_symbols[];
