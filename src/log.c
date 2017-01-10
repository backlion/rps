#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>


static struct logger logger;


void
_log_stream(FILE *stream, const char *fmt, ...) {
    char buf[LOG_MAX_LEN];
    va_list args;
    int len;
    
    len = 0;

    va_start(args, fmt);
    len += vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    buf[len++] = '\n';
    
    fwrite(buf, 1, len, stream);
}

void
_log(log_level level, const char *file, int line, const char *fmt, ...) {
    struct logger *l = &logger;
    int len;
    size_t size;
    char buf[LOG_MAX_LEN];
    va_list args;
    struct timeval  tv;
    
    if (l->fd == NULL) {
        return;
    }

    if (l->level < level) {
        return;
    }

    len = 0;
    size = LOG_MAX_LEN;

    gettimeofday(&tv, NULL);
    buf[len++] = '[';
    len += strftime(buf + len, size - len, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
    len += snprintf(buf + len, size - len, "] <%s:%d> ", file, line);
    len += snprintf(buf + len, size - len, "%s: ", log_level_to_text(level)); 

    va_start(args, fmt);
    len += vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    fwrite(buf, 1, len, l->fd);
}

int
log_set_level(log_level level) {
    struct logger *l = &logger;
    
    if (level < LOG_CRITICAL || level > LOG_VERBOSE) {
        log_stderr("invalid log level <%d>", level);
        return -1;
    }
    l->level = level;
    
    return 0;
}

int
log_set_output(char *fname) {
    struct logger *l = &logger;
       
    if (fname == NULL || !(strlen(fname))) {
        l->fd = stdout;
    } else {
        l->fd = fopen(fname, "a");
        if (l->fd == NULL) {
            log_stderr("opening log file '%s' failed: %s", fname, strerror(errno));
            return -1;
        }
    }

    return 0;
}

int
log_init(log_level level, char *fname) {
    int status;
    
    status = log_set_level(level);
    if (status != 0) {
        return status;
    }

    status = log_set_output(fname);
    if (status != 0) {
        return status;
    }

    return 0;
}

void
log_deinit() {
    struct logger *l = &logger;
    
    if (l->fd == NULL || l->fd == stdout || l->fd == stderr) {
        return;
    }
    
    fclose(l->fd);
}

#ifdef LOG_TEST
int
main(int argc, char **argv) {
    int stat;
    log_level level;

    level = log_level_to_int("DEBUG");
    
    stat = log_init(level, NULL);
    if (stat != 0) {
        log_stderr("init log failed.");
        exit(1);
    }
    
    log_debug("Hello %s", "kenshin");
}
#endif

