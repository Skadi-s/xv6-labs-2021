#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

// strdup替代实现
char* 
my_strdup(const char* s) {
    int len = strlen(s) + 1;
    char* p = malloc(len);
    if (p)
        memmove(p, s, len);
    return p;
}

int 
main (int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs command [args...]\n");
        exit(1);
    }
    // Construct command and arguments
    char *cmd = argv[1];
    char *args[MAXARG];
    // Copy fixed arguments
    for (int i = 1; i < argc; i++) {
        args[i - 1] = argv[i];
    }
    args[argc - 1] = 0;
    int arg_count = argc - 1;

    // read from stdin byte by byte
    int n;
    int i = 0;
    char buf[512];
    while (read(0, &n, 1) == 1) {
        if (n == ' ' || n == '\n') {
            if (i > 0) {
                buf[i] = 0; // null terminate
                    args[arg_count++] = my_strdup(buf);
                i = 0;
                if (arg_count >= MAXARG - 1 || n == '\n') {
                    // execute command
                    args[arg_count] = 0;
                    if (fork() == 0) {
                        exec(cmd, args);
                        fprintf(2, "exec %s failed\n", cmd);
                        exit(1);
                    } else {
                        wait(0);
                    }
                    // reset for next batch
                    arg_count = argc - 1;
                }
            }
        } else {
            buf[i++] = n;
        }
    }
    // handle last argument if any
    if (i > 0) {
        buf[i] = 0;
        args[arg_count++] = my_strdup(buf);
    }
    if (arg_count > argc - 1) {
        args[arg_count] = 0;
        if (fork() == 0) {
            exec(cmd, args);
            fprintf(2, "exec %s failed\n", cmd);
            exit(1);
        } else {
            wait(0);
        }
    }

    exit(0);
}