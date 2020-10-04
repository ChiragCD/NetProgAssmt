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

int store_chunk (msg message);
int copy_chunk (msg message);
int remove_chunk (msg message);
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
        if(recv_buf.mbody.req == STORE_CHUNK) status = store_chunk(recv_buf);
        if(recv_buf.mbody.req == COPY_CHUNK) status = copy_chunk(recv_buf);
        if(recv_buf.mbody.req == REMOVE_CHUNK) status = remove_chunk(recv_buf);
        if(recv_buf.mbody.req == COMMAND) status = command(recv_buf);
        if(recv_buf.mbody.req == STATUS_UPDATE) status = status_update(recv_buf);
    }
}

int main(int argc, char ** argv) {
    int CHUNK_SIZE = atoi(argv[1]);
    d_server(CHUNK_SIZE);
    return 0;
}

int store_chunk (msg message) {

    printf("\nStarting store chunk\n");

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

    write(fd, message.mbody.chunk.data, MSGSIZE/2);

    strcpy(send.mbody.error, "Store Success");
    send.mbody.status = 0;
    msgsnd(mqid, &send, MSGSIZE, 0);
    printf("Stored chunk %d\n", chunk_id);

    close(fd);
    return 0;
}

int copy_chunk (msg message) {

    printf("\nStarting copy chunk\n");

    int chunk_id = message.mbody.chunk.chunk_id;
    int new_chunk_id = message.mbody.status;
    pid_t new_server = message.mbody.addresses[0];

    char buffer[100];
    get_file_name(chunk_id, buffer);
    int fd = open(buffer, O_RDONLY, 0777);
    if(fd == -1) {
        printf("Not found\n");
        return -1;
    }

    msg send;
    send.mtype = new_server;
    send.mbody.sender = getpid();
    send.mbody.req = STORE_CHUNK;
    send.mbody.chunk.chunk_id = new_chunk_id;
    int num_read;
    num_read = read(fd,send.mbody.chunk.data,MSGSIZE/2);
    close(fd);
    send.mbody.chunk.data[num_read] = '\0';
    msgsnd(mqid, &send, MSGSIZE, 0);
    msgrcv(mqid, &send, MSGSIZE/2, getpid(), 0);
    printf("Copy complete\n");
    return 0;
}

int remove_chunk (msg message) {

    printf("\nStarting remove chunk\n");

    int chunk_id = message.mbody.chunk.chunk_id;
    char buffer[100];
    get_file_name(chunk_id, buffer);
    int fd = open(buffer, O_RDONLY, 0777);
    if(fd == -1) {
        printf("Chunk not present\n");
        return -1;
    }
    close(fd);
    remove(buffer);
    printf("Successfully removed %d\n", chunk_id);
    return 0;
}

int command (msg message) {
    msg send_buf;
    send_buf.mtype = message.mbody.sender;
    send_buf.mbody.sender = getpid();
    char fname[100];
    strcpy(fname,dir_name);
    strcat(fname,"/chunk");
    strcat(fname,message.mbody.paths[0]);
    strcat(fname,".txt");
    printf("Attempting to open %s\n",fname);
    int fd;
    if((fd = open(fname,O_RDONLY)) == -1){
        send_buf.mbody.status=-1;
        strcpy(send_buf.mbody.error,"COULDN'T OPEN FILE INSIDE D SERVER\n");
        msgsnd(mqid,&send_buf,MSGSIZE,0);
        printf("COULDN'T OPEN\n");
        return -1;
    }
    char actual_cmd[100];
    strcpy(actual_cmd,message.mbody.paths[1]);
    char* token = strtok(actual_cmd," ");
    char cmd[100];
    strcpy(cmd,  token);
    char* args[20];
    int m=1;
    args[0] = cmd;
    while(token!=NULL){
        token=strtok(NULL," ");
        args[m++] = token;
    }
    args[m] = NULL;
    int arr[2];
    pipe(arr);
    printf("About to execute %s on %s\n",cmd,fname);
    if(fork()){// parent
    close(arr[1]); // close write end
    send_buf.mbody.req = OUTPUT;
    //fscanf(arr[0],"%s",send_buf.mbody.error);
    int n = read(arr[0],send_buf.mbody.error,100);
    send_buf.mbody.error[n] = '\0';
    printf("Successfully execd, output is:  %s .\n",send_buf.mbody.error);
    msgsnd(mqid,&send_buf,MSGSIZE,0);
    return 0;
    }
    dup2(1,arr[1]); //duplicate write end for child
    close(arr[0]);  //close the read end for child
    dup2(0,fd);
    execvp(cmd,args);
}

int status_update (msg message) {
    printf("\nReceived update from %d - %s\n", message.mbody.sender, message.mbody.error);
    return 0;
}
