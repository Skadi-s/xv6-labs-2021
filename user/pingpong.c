#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int p1[2];  // parent to child
    int p2[2];  // child to parent
    pipe(p1);
    pipe(p2);
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error\n");
        exit(1);
    } else if (pid == 0) {
        // Child process
        close(p1[1]); // Close write end
        close(p2[0]); // Close read end
        // read one byte from pipe
        char buf[1];
        read(p1[0], buf, 1);
        printf("%d: received ping\n", getpid());
        write(p2[1], "p", 1);
        close(p1[0]); // Close read end
        close(p2[1]); // Close write end
        exit(0);
    } else {
        // Parent process
        close(p1[0]); // Close read end
        close(p2[1]); // Close write end
        // write one byte to pipe
        char buf[1];
        write(p1[1], "p", 1);
        read(p2[0], buf, 1);
        printf("%d: received pong\n", getpid());
        close(p1[1]); // Close write end
        close(p2[0]); // Close read end
        wait(0); // Wait for child to finish
        exit(0);
    }
}