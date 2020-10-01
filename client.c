#include "msg.h"

static int mqid;

void siginthandler(int status) {
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);
    msgctl(mqid, IPC_RMID, NULL);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
    return;
}

void client() {
    
    char cwd[200];
    getcwd(cwd, sizeof(cwd));
    key_t key = ftok(cwd, 42);
    mqid = msgget(key, S_IWUSR | S_IRUSR);

    char msgs[10][10] = {
    "Message 1",
    "Message 2",
    "Message 3",
    "Message 4",
    "Message 5",
    "Message 6",
    "Message 7",
    "Message 8",
    "Message 9",
    "Message10"
    };

    msg send_buf;
    send_buf.mtype = 1;
    send_buf.mbody.sender = getpid();
    for(int i = 0; i < 10; i++) {
        for(int j = 0; j < 10; j++) send_buf.mbody.mtext[j] = msgs[i][j];
        ssize_t temp = msgsnd(mqid, &send_buf, MSGSIZE, 0);
        printf("%s\n", send_buf.mbody.mtext);
    }
}

int main(int argc, char ** argv) {
    client();
    return 0;
}