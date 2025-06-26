#include "utils.h"
#include <string.h>

void trim_newline(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r' || str[len-1] == ' ')) {
        str[--len] = '\0';
    }
}
