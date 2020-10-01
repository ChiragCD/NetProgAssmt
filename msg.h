#ifndef MSG_H
#define MSG_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <sys/stat.h>

#define MSGSIZE 1024

typedef enum request {
    STATUS_UPDATE,
    ADD_FILE,
    ADD_CHUNK,
    STORE_CHUNK,
    NOTIFY_EXISTENCE,
    CP,
    COPY_CHUNK,
    MV,
    RM,
    REMOVE_CHUNK,
    COMMAND,
    OUTPUT
} request;

typedef struct {
    request req;
    pid_t sender;
    pid_t receiver;
    int status;
    pid_t addresses[3];
    int chunk_num;
    char mtext[MSGSIZE];
} msg_body;

typedef struct msg {
    long mtype;
    msg_body mbody;
} msg;

typedef struct file {
    struct file * next;
    int hash;
    int num_chunks;
    int chunk_ids[1024];
} file;

typedef struct storage {
    file * heads[16];
} storage;

typedef struct chunk_locs {
    pid_t locations[3];
} chunk_locs;

typedef struct chunk {
    int chunk_id;
    char * data;
} chunk;


#endif