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
#include <fcntl.h>

#include <sys/stat.h>

#define MSGSIZE 4096
#define MAXCHUNKS 1024
#define NUMCOPIES 3

typedef enum request {
    STATUS_UPDATE,
    ADD_FILE,
    ADD_CHUNK,
    CHUNK_DATA,
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

typedef struct chunk {
    int chunk_id;
    char data[MSGSIZE*7/8];
} chunk;

typedef struct {
    request req;
    pid_t sender;
    int status;
    pid_t addresses[3];
    char paths[2][128];
    char error[128];
    chunk chunk;
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

typedef struct node {
    struct node * next;
    chunk * element;
} node;

typedef struct chunk_map {
    node * heads[16];
} chunk_map;


#endif