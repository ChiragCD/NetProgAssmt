#include "msg.h"

static int mqid;

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
    while (temp)
    {
        if(temp->element->chunk_id == id) return temp->element;
        temp = temp->next;
    }
    return NULL;
}

chunk * rem(chunk_map * map, int id) {
    node * temp;
    node * pointer = map->heads[id%16];
    if(pointer && pointer->element->chunk_id == id) {
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

int store_chunk (msg message);
int copy_chunk (msg message, chunk_map * map);
int remove_chunk (msg message, chunk_map * map);
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

    chunk_map map;
    clear(&map);

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
        if(recv_buf.mbody.req == COPY_CHUNK) status = copy_chunk(recv_buf, &map);
        if(recv_buf.mbody.req == REMOVE_CHUNK) status = remove_chunk(recv_buf, &map);
        if(recv_buf.mbody.req == COMMAND) status = command(recv_buf);
        printf("%s\n", recv_buf.mbody.chunk.data);
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

int copy_chunk (msg message, chunk_map * map) {
    int chunk_id = message.mbody.chunk.chunk_id;
    int new_chunk_id = message.mbody.status;
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
    *writer = *reader;
    msgsnd(mqid, &send, MSGSIZE, 0);
    return 0;
}

int remove_chunk (msg message, chunk_map * map) {
    int chunk_id = message.mbody.chunk.chunk_id;
    chunk * c = rem(map, chunk_id);
    if(c == NULL) return -1;
    free(c);
    return 0;
}

int command (msg message) {
    ;
}