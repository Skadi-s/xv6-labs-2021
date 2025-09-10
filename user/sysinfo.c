#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/sysinfo.h"

int
main(int argc, char *argv[])
{
    struct sysinfo info;
    if(sysinfo(&info) < 0){
        fprintf(2, "sysinfo error\n");
        exit(1);
    }
    printf("freemem: %d bytes\n", info.freemem);
    printf("nproc: %d\n", info.nproc);
    exit(0);
}