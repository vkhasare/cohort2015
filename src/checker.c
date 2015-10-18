#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]){
    char cmd[256];
    sprintf(cmd, "%s", argv[1]);
    strcat(cmd," --version > /dev/null 2>&1");
    int ret = system(cmd);
    if (ret == 0) {
        return 0;
            //The executable was found.
    }
    return 1;
}
