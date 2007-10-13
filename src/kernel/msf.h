#include <stddef.h>

extern const char model_symbols[];
extern const size_t model_symbols_len;

struct other_files {
    char *file_name;
    size_t len;
    char *data;
};

extern const size_t other_files_cnt;
extern const struct other_files other_files[];
