#include "msg.h"

static int mqid;
static char dir_name[10];

void get_file_name(int chunk_id, char * buffer) {
    strcpy(buffer, dir_name);
    buffer += strlen(buffer);
    strcpy(buffer, "/chunk");
    sprintf(buffer+6, "%d", chunk_id);
    buffer += strlen(buffer);
    strcpy(buffer, ".txt");
}

void clear (chunk_map * map) {
    for(int i = 0; i < 16; i++) map->heads[i] = NULL;
}

int add(chunk_map * map, chunk * c) {
    node * n = (node *) malloc(sizeof(node));
    n->element = c;
    n->next = map->heads[c->chunk_id%16];
    map->heads[c->chunk_id%16] = n;
    return 0;
}

chunk * get(chunk_map * map, int id) {
    node * temp = map->heads[id%16];
    while (temp) {
        if(temp->element->chunk_id == id) return temp->element;
        temp = temp->next;
    }
    return NULL;
}

chunk * rem(chunk_map * map, int id) {
    node * pointer = map->heads[id%16];
    if(!pointer) return NULL;
    if(pointer->element->chunk_id == id) {
        chunk * temp = pointer->element;
        map->heads[id%16] = pointer->next;
        free(pointer);
        return temp;
    }
    while(pointer->next) {
        if(pointer->next->element->chunk_id == id) {
            chunk * temp = pointer->next->element;
            node * freeable = pointer->next;
            pointer->next = freeable->next;
            free(freeable);
            return temp;
        }
        pointer = pointer->next;
    }
    return NULL;
}

int store_chunk (msg message, chunk_map * map);
int copy_chunk (msg message, chunk_map * map);
int remove_chunk (msg message, chunk_map * map);
int command (msg message);
int status_update (msg message);

void d_server() {

    pid_t pid = getpid();
    printf("D server at %d\n", pid);
    
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

    chunk_map map;
    clear(&map);

    send_buf.mbody.sender = getpid();

    send_buf.mtype = 1;
    send_buf.mbody.req = NOTIFY_EXISTENCE;
    msgsnd(mqid, &send_buf, MSGSIZE, 0);

    dir_name[0] = 'D';
    sprintf(dir_name, "%d", pid);
    mkdir(dir_name, 0777);

    for(;;) {
        ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, pid, 0);
        if(msgsize == -1) {
            perror("Recv ");
            raise(SIGINT);
            exit(1);
        }
        int status = 0;
        if(recv_buf.mbody.req == STORE_CHUNK) status = store_chunk(recv_buf, &map);
        if(recv_buf.mbody.req == COPY_CHUNK) status = copy_chunk(recv_buf, &map);
        if(recv_buf.mbody.req == REMOVE_CHUNK) status = remove_chunk(recv_buf, &map);
        if(recv_buf.mbody.req == COMMAND) status = command(recv_buf);
        if(recv_buf.mbody.req == STATUS_UPDATE) status = status_update(recv_buf);
    }
}

int main(int argc, char ** argv) {
    int CHUNK_SIZE = atoi(argv[1]);
    d_server(CHUNK_SIZE);
    return 0;
}

int store_chunk (msg message, chunk_map * map) {

    printf("\nStarting store chunk %s\n", message.mbody.chunk.data);

    int chunk_id = message.mbody.chunk.chunk_id;
    msg send;
    send.mtype = message.mbody.sender;
    send.mbody.sender = getpid();
    send.mbody.req = STATUS_UPDATE;

    char buffer[100];
    get_file_name(chunk_id, buffer);
    int fd = open(buffer, O_CREAT|O_EXCL|O_RDWR, 0777);
    if(fd == -1) {
        send.mbody.status = -1;
        strcpy(send.mbody.error, "Chunk already present\n");
        msgsnd(mqid, &send, MSGSIZE, 0);
        printf("Store chunk error - Chunk %d already present\n", chunk_id);
        return -1;
    }

    chunk * c = (chunk *) malloc(sizeof(chunk));
    c->chunk_id = chunk_id;
    write(fd, message.mbody.chunk.data, MSGSIZE/2);

    strcpy(send.mbody.error, "Store Success");
    send.mbody.status = 0;
    msgsnd(mqid, &send, MSGSIZE, 0);
    printf("Stored chunk %d\n", c->chunk_id);

    close(fd);
    return 0;
}

int copy_chunk (msg message, chunk_map * map) {

    printf("\nStarting copy chunk\n");

    int chunk_id = message.mbody.chunk.chunk_id;
    int new_chunk_id = message.mbody.status;
    printf("%d to %d\n", chunk_id, new_chunk_id);
    pid_t new_server = message.mbody.addresses[0];
    chunk * c = get(map, chunk_id);
    if(c == NULL) return -1;

    msg send;
    send.mtype = new_server;
    send.mbody.sender = getpid();
    send.mbody.req = STORE_CHUNK;
    send.mbody.chunk.chunk_id = new_chunk_id;
    char * reader = c->data;
    char * writer = send.mbody.chunk.data;
    while(*reader && *reader != EOF) *(writer++) = *(reader++);
    printf("Loop complete\n");
    *writer = *reader;
    msgsnd(mqid, &send, MSGSIZE, 0);
    printf("Copy complete\n");
    return 0;
}

int remove_chunk (msg message, chunk_map * map) {

    printf("\nStarting remove chunk\n");

    int chunk_id = message.mbody.chunk.chunk_id;
    chunk * c = rem(map, chunk_id);
    if(c == NULL) {
        printf("Chunk not present\n");
        return -1;
    }
    free(c);
    printf("Successfully removed %d\n", chunk_id);
    return 0;
}

int command (msg message) {
    ;
}

int status_update (msg message) {
    printf("\nReceived update from %d - %s\n", message.mbody.sender, message.mbody.error);
    return 0;
}