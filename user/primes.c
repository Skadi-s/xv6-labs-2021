#include "kernel/types.h"
#include "user/user.h"

__attribute__((noreturn))
void 
prime_proc(int fd) {
    // read from left pipe and calculate
    int prime;
    if (read(fd, &prime, sizeof(prime)) == 0) {
        close(fd);
        exit(0);
    }
    printf("prime %d\n", prime);

    int p[2];
    pipe(p);

    int num;
    while (read(fd, &num, sizeof(num)) != 0) {
        if (num % prime != 0) {
            write(p[1], &num, sizeof(num));
        }
    }
    close(fd);
    close(p[1]);

    if (fork() == 0) {
        prime_proc(p[0]);
        exit(0);
    } else {
        close(p[0]);
        wait(0);
        exit(0);
    }
    
}

int 
main (int argc, char *argv[]) {
    int p[2];
    pipe(p);

    for (int i = 2; i <= 35; i++) {
        write(p[1], &i, sizeof(i));
    }
    close(p[1]);

    if (fork() == 0) {
        prime_proc(p[0]);
        exit(0);
    } else {
        close(p[0]);
        wait(0);
        exit(0);
    }
}
