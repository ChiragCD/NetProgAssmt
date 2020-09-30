#include <stdlib.h>
#include <stdio.h>

void shell(int argc, char ** argv) {
    while(1) {
        accept();
        execute();
    }
    return;
}

int main (int argc, char ** argv) {
    shell(argc, argv);
    return 0;
}