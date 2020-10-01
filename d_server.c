#include "msg.h"

static int mqid;

int store_chunk (msg message);
int copy_chunk (msg message);
int remove_chunk (msg message);
int command (msg message);

void d_server() {

    pid_t pid = getpid();
    
    char cwd[200];
    getcwd(cwd, sizeof(cwd));
    key_t key = ftok(cwd, 42);
    mqid = msgget(key, S_IWUSR | S_IRUSR);
    if(mqid == -1) {
        perror("Msq not obtained");
        return;
    }

    msg recv_buf;
    msg send_buf;

    chunk array[1024];

    send_buf.mbody.sender = getpid();

    send_buf.mtype = 1;
    send_buf.mbody.req = NOTIFY_EXISTENCE;
    msgsnd(mqid, &send_buf, MSGSIZE, 0);

    for(;;) {
        ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, pid, 0);
        if(msgsize == -1) {
            perror("Recv ");
            raise(SIGINT);
            exit(1);
        }
        int status = 0;
        if(recv_buf.mbody.req == STORE_CHUNK) status = store_chunk(recv_buf);
        if(recv_buf.mbody.req == COPY_CHUNK) status = copy_chunk(recv_buf);
        if(recv_buf.mbody.req == REMOVE_CHUNK) status = remove_chunk(recv_buf);
        if(recv_buf.mbody.req == COMMAND) status = command(recv_buf);
        printf("%s\n", recv_buf.mbody.mtext);
    }
}

int main(int argc, char ** argv) {
    int CHUNK_SIZE = atoi(argv[1]);
    d_server(CHUNK_SIZE);
    return 0;
}

int store_chunk (msg message) {
    ;
}

int copy_chunk (msg message) {
    ;
}

int remove_chunk (msg message) {
    ;
}

int command (msg message) {
    ;
}