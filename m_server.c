#include "msg.h"

static int mqid;

void clear(storage * file_index) {
    for(int i = 0; i < 16; i++) file_index->heads[i] = NULL;
}

int check_if(storage * file_index, int hash) {
    file * pointer = file_index->heads[hash%16];
    while(pointer) {
        if(pointer->hash == hash) return 1;
        pointer = pointer->next;
    }
    return 0;
}

int add(storage * file_index, file * f) {
    if(check_if(file_index, f->hash)) return -1;
    f->next = file_index->heads[f->hash%16];
    file_index->heads[f->hash%16] = f;
    return 0;
}

file * rem(storage * file_index, int hash) {
    file * temp;
    if(!check_if(file_index, hash)) return NULL;
    file * pointer = file_index->heads[hash%16];
    if(pointer->hash == hash) {
        temp = pointer;
        file_index->heads[hash%16] = pointer->next;
        return temp;
    }
    while(pointer->next) {
        if(pointer->next->hash == hash) {
            file * temp = pointer->next;
            pointer->next = temp->next;
            return temp;
        }
        pointer = pointer->next;
    }
    return NULL;
}

int add_file (msg message, storage * file_index);
int notify_existence (msg message, pid_t * d_array, int * num_d);
int add_chunk (msg message);
int cp (msg message);
int mv (msg message, storage * file_index);
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
    clear(&file_index);
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
        if(recv_buf.mbody.req == MV) status = mv(recv_buf, &file_index);
        if(recv_buf.mbody.req == RM) status = rm(recv_buf);
        if(recv_buf.mbody.req == STATUS_UPDATE) status = status_update(recv_buf);
        printf("%s\n", recv_buf.mbody.chunk.data);
    }
}

int add_file (msg message, storage * file_index) {

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = getpid();
    send.mbody.req = STATUS_UPDATE;

    file * new = (file *) malloc(sizeof(file));
    new->hash = ftok(message.mbody.paths[0], 42);
    new->num_chunks = 0;

    int error = add(file_index, new);
    if(error == -1) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Add Error - file already exists at location");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Success");
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

int mv (msg message, storage * file_index) {

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = getpid();
    send.mbody.req = STATUS_UPDATE;

    int hash = ftok(message.mbody.paths[0], 42);
    file * f = rem(file_index, hash);

    if(f == NULL) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Move Error - file does not exist");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }

    int new_hash = ftok(message.mbody.paths[1], 42);
    f->hash = new_hash;

    int error = add(file_index, f);
    if(error == -1) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Move Error - file already exists at new location, no change");
        msgsnd(mqid, &send, MSGSIZE, 0);
        f->hash = hash;
        add(file_index, f);
        return -1;
    }
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
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