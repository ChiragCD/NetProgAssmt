#include "msg.h"

static int mqid;

int add_file (msg message, storage * file_index);
int notify_existence (msg message, pid_t * d_array, int * num_d);
int add_chunk (msg message);
int cp (msg message);
int mv (msg message);
int rm (msg message);
int status_update (msg message);

void siginthandler(int status) {
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);
    msgctl(mqid, IPC_RMID, NULL);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
    exit(0);
}

void m_server(int CHUNK_SIZE) {
    
    char cwd[200];
    getcwd(cwd, sizeof(cwd));
    key_t key = ftok(cwd, 42);
    mqid = msgget(key, IPC_CREAT | IPC_EXCL | S_IWUSR | S_IRUSR);
    if(mqid == -1) {
        perror("Msq creation");
        return;
    }

    msg recv_buf;
    msg send_buf;

    storage file_index;
    for(int i = 0; i < 16; i++) file_index.heads[i] = NULL;
    int chunk_index_size = 16;
    chunk_locs * chunk_index = (chunk_locs *) malloc(chunk_index_size * sizeof(chunk_locs));

    int num_d_servers;
    pid_t d_servers[1024];

    for(;;) {
        ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, 1, 0);
        if(msgsize == -1) {
            perror("Recv ");
            raise(SIGINT);
            exit(1);
        }
        int status = 0;
        if(recv_buf.mbody.req == ADD_FILE) status = add_file(recv_buf, &file_index);
        if(recv_buf.mbody.req == NOTIFY_EXISTENCE) status = notify_existence(recv_buf, d_servers, &num_d_servers);
        if(recv_buf.mbody.req == ADD_CHUNK) status = add_chunk(recv_buf);
        if(recv_buf.mbody.req == CP) status = cp(recv_buf);
        if(recv_buf.mbody.req == MV) status = mv(recv_buf);
        if(recv_buf.mbody.req == RM) status = rm(recv_buf);
        if(recv_buf.mbody.req == STATUS_UPDATE) status = status_update(recv_buf);
        printf("%s\n", recv_buf.mbody.mtext);
    }
}

int add_file (msg message, storage * file_index) {

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = getpid();
    send.mbody.req = STATUS_UPDATE;

    int hash = ftok(message.mbody.mtext, 42);
    file * pointer = file_index->heads[hash%16];
    while(pointer) {
        if(pointer->hash == hash) {
            send.mbody.status = -1;
            strcpy(send.mbody.mtext, "Error - file already exists at location");
            msgsnd(mqid, &send, MSGSIZE, 0);
            return -1;
        }
        pointer = pointer->next;
    }
    file * new = (file *) malloc(sizeof(file));
    new->next = file_index->heads[hash%16];
    new->hash = hash;
    new->num_chunks = 0;
    file_index->heads[hash%16] = new;
    send.mbody.status = 0;
    strcpy(send.mbody.mtext, "Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int notify_existence (msg message, pid_t * d_array, int * num_d) {
    printf("Received D server at %d\n", message.mbody.sender);
    d_array[*num_d] = message.mbody.sender;
    (*num_d)++;
    return 0;
}

int add_chunk (msg message) {
    ;
}

int cp (msg message) {
    ;
}

int mv (msg message) {
    ;
}

int rm (msg message) {
    ;
}

int status_update (msg message) {
    ;
}

int main(int argc, char ** argv) {
    signal(SIGINT, siginthandler);
    int CHUNK_SIZE = atoi(argv[1]);
    m_server(CHUNK_SIZE);
    return 0;
}