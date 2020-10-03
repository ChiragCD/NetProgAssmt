#include "msg.h"

static int mqid;
static int chunk_counter;

int name_server() {
    chunk_counter++;
    return chunk_counter;
}

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

file * get(storage * file_index, int hash) {
    file * temp = file_index->heads[hash%16];
    while (temp)
    {
        if(temp->hash == hash) return temp;
        temp = temp->next;
    }
    return NULL;
}

file * rem(storage * file_index, int hash) {
    file * temp;
    file * pointer = file_index->heads[hash%16];
    if(pointer && pointer->hash == hash) {
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
int add_chunk (msg message, storage * file_index, pid_t ** chunk_index, pid_t * d_servers, int num_servers);
int cp (msg message, storage * file_index, pid_t ** chunk_index, pid_t * d_servers, int num_servers);
int mv (msg message, storage * file_index);
int rm (msg message, storage * file_index, pid_t ** chunk_index);
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

    chunk_counter = 0;
    
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
    pid_t chunk_locs[MAXCHUNKS][NUMCOPIES];

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
        if(recv_buf.mbody.req == ADD_CHUNK) status = add_chunk(recv_buf, &file_index, chunk_locs, d_servers, num_d_servers);
        if(recv_buf.mbody.req == CP) status = cp(recv_buf, &file_index, chunk_locs, d_servers, num_d_servers);
        if(recv_buf.mbody.req == MV) status = mv(recv_buf, &file_index);
        if(recv_buf.mbody.req == RM) status = rm(recv_buf, &file_index, chunk_locs);
        if(recv_buf.mbody.req == STATUS_UPDATE) status = status_update(recv_buf);
        printf("%s\n", recv_buf.mbody.chunk.data);
    }
}

int add_file (msg message, storage * file_index) {

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = STATUS_UPDATE;

    file * new = (file *) malloc(sizeof(file));
    new->hash = ftok(message.mbody.paths[0], 42);
    new->num_chunks = 0;

    int error = add(file_index, new);
    if(error == -1) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Add file Error - file already exists at location");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Add file Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int notify_existence (msg message, pid_t * d_array, int * num_d) {
    printf("Received D server at %d\n", message.mbody.sender);
    d_array[*num_d] = message.mbody.sender;
    (*num_d)++;
    return 0;
}

int add_chunk (msg message, storage * file_index, pid_t ** chunk_index, pid_t * d_servers, int num_servers) {
    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = CHUNK_DATA;

    int hash = ftok(message.mbody.paths[0], 42);
    file * f = get(file_index, hash);
    if(!f) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Add chunk Error - file does not exist");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }

    int new_chunk_id = name_server();
    send.mbody.chunk.chunk_id = new_chunk_id;
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Add chunk Success");
    f->chunk_ids[f->num_chunks] = new_chunk_id;
    (f->num_chunks)++;
    for(int i = 0; i < NUMCOPIES; i++) {
        chunk_index[new_chunk_id][i] = d_servers[rand()%num_servers];
        send.mbody.addresses[i] = chunk_index[new_chunk_id][i];
    }
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int cp (msg message, storage * file_index, pid_t ** chunk_index, pid_t * d_servers, int num_servers) {
    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = STATUS_UPDATE;

    int hash = ftok(message.mbody.paths[0], 42);
    int new_hash = ftok(message.mbody.paths[1], 42);
    file * f = get(file_index, hash);
    if(f == NULL) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Copy Error - file does not exist");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }
    if(get(file_index, new_hash)) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Copy Error - file already exists at new location, no change");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }
    file * new = (file *) malloc(sizeof(file));
    new->hash = new_hash;
    new->num_chunks = f->num_chunks;

    send.mbody.req = COPY_CHUNK;
    for(int i = 0; i < new->num_chunks; i++) {
        int current_chunk = f->chunk_ids[i];
        int new_chunk = name_server();
        new->chunk_ids[i] = new_chunk;
        for(int j = 0; j < NUMCOPIES; j++) {
            pid_t new_server = d_servers[rand()%num_servers];
            chunk_index[new_chunk][j] = new_server;
            send.mtype = chunk_index[current_chunk][j];
            send.mbody.status = new_chunk;
            send.mbody.addresses[0] = new_server;
            send.mbody.chunk.chunk_id = current_chunk;
            msgsnd(mqid, &send, MSGSIZE, 0);
        }
    }

    add(file_index, new);

    send.mtype = message.mbody.sender;
    send.mbody.req = STATUS_UPDATE;
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Copy Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int mv (msg message, storage * file_index) {

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
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
    strcpy(send.mbody.error, "Move Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int rm (msg message, storage * file_index, pid_t ** chunk_index) {
    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = STATUS_UPDATE;

    int hash = ftok(message.mbody.paths[0], 42);
    file * f = rem(file_index, hash);
    if(f == NULL) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Remove Error - file does not exist");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }

    send.mbody.req = REMOVE_CHUNK;
    for(int i = 0; i < f->num_chunks; i++) {
        int chunk = f->chunk_ids[i];
        send.mbody.chunk.chunk_id = chunk;
        for(int j = 0; j < NUMCOPIES; j++) {
            pid_t server = chunk_index[chunk][j];
            send.mtype = server;
            msgsnd(mqid, &send, MSGSIZE, 0);
        }
    }
    free(f);

    send.mtype = message.mbody.sender;
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Remove Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int status_update (msg message) {
    printf("Received update from %d - %s\n", message.mbody.sender, message.mbody.error);
    return 0;
}

int main(int argc, char ** argv) {
    signal(SIGINT, siginthandler);
    int CHUNK_SIZE = atoi(argv[1]);
    m_server(CHUNK_SIZE);
    return 0;
}