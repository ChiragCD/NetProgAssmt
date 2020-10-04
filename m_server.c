#include "msg.h"

static int mqid;
static int chunk_counter;

int name_server() {
    chunk_counter++;
    return chunk_counter;
}

int hash_func(char str[]) {
    // djb2 hashing
    int hash = 5381;
    char c;
    while(c = *str) {
        hash = (((hash << 5) + hash) + c)%(1<<5);
        str++;
    }
    return hash;
}

void clear(storage * file_index) {
    for(int i = 0; i < 16; i++) file_index->heads[i] = NULL;
}

int check_if(storage * file_index, int hash) {
    file * pointer = (file_index->heads)[hash%16];
    while(pointer) {
        if(pointer->hash == hash) return 1;
        pointer = pointer->next;
    }
    return 0;
}

int add(storage * file_index, file * f) {
    if(check_if(file_index, f->hash)) return -1;
    f->next = (file_index->heads)[f->hash%16];
    (file_index->heads)[f->hash%16] = f;
    return 0;
}

file * get(storage * file_index, int hash) {
    file * temp = (file_index->heads)[hash%16];
    while (temp)
    {
        //printf("Looking through hashmap at element with HASH:%d\n",temp->hash);
        if(temp->hash == hash) return temp;
        temp = temp->next;
    }
    return NULL;
}

file * rem(storage * file_index, int hash) {
    file * temp;
    file * pointer = (file_index->heads)[hash%16];
    if(!pointer) return NULL;
    if(pointer->hash == hash) {
        temp = pointer;
        (file_index->heads)[hash%16] = pointer->next;
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
int cp (msg message, storage * file_index, pid_t * chunk_index[], pid_t d_servers[], int num_servers);
int mv (msg message, storage * file_index);
int rm (msg message, storage * file_index, pid_t * chunk_index[]);
int status_update (msg message);

void siginthandler(int status) {
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);
    msgctl(mqid, IPC_RMID, NULL);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
    exit(0);
}

void m_server() {

    chunk_counter = 0;
    
    char cwd[200];
    getcwd(cwd, sizeof(cwd));
    key_t key = ftok(cwd, 42);
    printf("MQ - %d\n", key);
    mqid = msgget(key, IPC_CREAT | IPC_EXCL | S_IWUSR | S_IRUSR);
    if(mqid == -1) {
        perror("Msq creation");
        return;
    }

    msg recv_buf;
    msg send_buf;

    storage file_index;
    clear(&file_index);
    pid_t ** chunk_locs = (pid_t **) malloc(MAXCHUNKS * sizeof(pid_t *));
    for(int i = 0; i < MAXCHUNKS; i++) chunk_locs[i] = (pid_t *) malloc(NUMCOPIES * sizeof(pid_t));

    int num_d_servers = 0;
    pid_t * d_servers = (pid_t *) malloc(1024 * sizeof(pid_t));

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
    }
}

int add_file (msg message, storage * file_index) {

    printf("\nStarting add file\n");

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = STATUS_UPDATE;

    file * new = (file *) malloc(sizeof(file));
    //printf("Getting hash for:%s,%d\n",message.mbody.paths[0],hash_func(message.mbody.paths[0]));
    new->hash = hash_func(message.mbody.paths[0]);
    new->num_chunks = 0;

    int error = add(file_index, new);
    if(error == -1) {
        printf("Add file Error - file already exists at location\n");
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Add file Error - file already exists at location");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }
    printf("Add file Success\n");
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Add file Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int notify_existence (msg message, pid_t * d_array, int * num_d) {
    printf("\nAdded D server, ID %d\n", message.mbody.sender);
    d_array[*num_d] = message.mbody.sender;
    (*num_d)++;
    return 0;
}

int add_chunk (msg message, storage * file_index, pid_t ** chunk_index, pid_t * d_servers, int num_servers) {

    printf("\nStarting add chunk\n");

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = CHUNK_DATA;
    //printf("Getting hash for:%s,%d\n",message.mbody.paths[0],hash_func(message.mbody.paths[0]));
    int hash = hash_func(message.mbody.paths[0]);
    file * f = get(file_index, hash);
    if(!f) {
        printf("Add chunk Error - file does not exist\n");
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Add chunk Error - file does not exist");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }
    if(!num_servers) {
        printf("Add chunk Error - no data servers\n");
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Add chunk Error - no data servers");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }

    int new_chunk_id = name_server();
    send.mbody.chunk.chunk_id = new_chunk_id;
    f->chunk_ids[f->num_chunks] = new_chunk_id;
    (f->num_chunks)++;
    for(int i = 0; i < NUMCOPIES; i++) {
        chunk_index[new_chunk_id][i] = d_servers[rand()%num_servers];
        send.mbody.addresses[i] = chunk_index[new_chunk_id][i];
    }
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Add chunk Success");
    printf("Added chunk %d successfully\n", new_chunk_id);
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int cp (msg message, storage * file_index, pid_t * chunk_index[], pid_t d_servers[], int num_servers) {

    printf("\nStarting copy\n");
    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = STATUS_UPDATE;

    int hash = hash_func(message.mbody.paths[0]);
    int new_hash = hash_func(message.mbody.paths[1]);
    file * f = get(file_index, hash);
    if(f == NULL) {
        printf("Copy Error - file does not exist\n");
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Copy Error - file does not exist");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }
    if(get(file_index, new_hash)) {
        printf("Copy Error - file already exists at new location, no change\n");
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
    printf("Copy success\n");

    send.mtype = message.mbody.sender;
    send.mbody.req = STATUS_UPDATE;
    send.mbody.status = 0;
    strcpy(send.mbody.error, "Copy Success");
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int mv (msg message, storage * file_index) {

    printf("\nStarting move\n");

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = STATUS_UPDATE;

    int hash = hash_func(message.mbody.paths[0]);
    file * f = rem(file_index, hash);

    if(f == NULL) {
        printf("Move Error - file does not exist\n");
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Move Error - file does not exist");
        msgsnd(mqid, &send, MSGSIZE, 0);
        return -1;
    }

    int new_hash = hash_func(message.mbody.paths[1]);
    f->hash = new_hash;

    int error = add(file_index, f);
    if(error == -1) {
        printf("Move Error - file already exists at new location, no change\n");
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
    printf("Move success\n");
    return 0;
}

int rm (msg message, storage * file_index, pid_t * chunk_index[]) {

    printf("\nStarting remove\n");

    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = 1;
    send.mbody.req = STATUS_UPDATE;

    int hash = hash_func(message.mbody.paths[0]);
    file * f = rem(file_index, hash);
    if(f == NULL) {
        printf("Remove Error - file does not exist\n");
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
    printf("Remove success\n");
    return 0;
}

int status_update (msg message) {
    printf("\nReceived update from %d - %s\n", message.mbody.sender, message.mbody.error);
    return 0;
}

int main(int argc, char ** argv) {
    signal(SIGINT, siginthandler);
    m_server();
    return 0;
}
