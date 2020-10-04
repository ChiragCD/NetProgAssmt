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
    msg send_buf,recv_buf;
    ssize_t temp, msgsize;
    for(;;){
            printf("Enter the type of operation you would like to perform:\n1. ADD FILE <file path>\n2. ADD CHUNK <file path> <machine file path> <chunk number>\n3. COPY <source> <destination>\n4. MOVE <source> <destination>\n5. REMOVE <file path>\n6. COMMAND <command> <chunk name>\n");
            int choice;
            choice = getchar();
            choice -= '0';
            char cmd[100],s[100],d[100],fp[100];
            int chunk_num;
            switch(choice){
                case 1:scanf("%s",s); // take in input the desired location in M.
                       send_buf.mtype=1;
                       send_buf.mbody.req = ADD_FILE;
                       send_buf.mbody.sender = getpid();
                       strcpy(send_buf.mbody.paths[0],s);
                       temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.status==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE ADDED SUCCESSFULLY\n");
                       break;
                case 2:scanf("%s",s); 
                       scanf("%s",fp); // get the local file path inside client file-directory
                       scanf("%d",&chunk_num);
                       send_buf.mtype=1;
                       send_buf.mbody.req = ADD_CHUNK;
                       send_buf.mbody.sender = getpid();
                       strcpy(send_buf.mbody.paths[0],s);
                       int fd;
                       if(fd = open(fp,O_RDONLY) == -1){
                               printf("Could not find file with name %s.\n",fp);
                               break;
                       }
                       chunk c;
                       lseek(fd,MSGSIZE*7/8*chunk_num,SEEK_SET);
                       if(read(fd,c.data,MSGSIZE*7/8) == 0 )
                       {printf("Chunk number too large, file is not that big\n");close(fd);break;}

                       temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.status==-1)
                       {printf("%s\n",recv_buf.mbody.error);close(fd);break;}
                       for(int i=0;i<3;i++){
                               pid_t d_pid = recv_buf.mbody.addresses[i];
                               send_buf.mtype=d_pid;
                               send_buf.mbody.chunk=c;
                               send_buf.mbody.chunk.chunk_id = recv_buf.mbody.chunk.chunk_id;
                               send_buf.mbody.req=STORE_CHUNK;
                               ssize_t temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       }
                       break;
                case 3:scanf("%s",s);
                       scanf("%s",d);
                       send_buf.mtype=1;
                       send_buf.mbody.req = CP;
                       send_buf.mbody.sender = getpid();
                       strcpy(send_buf.mbody.paths[0],s);
                       strcpy(send_buf.mbody.paths[1],d);
                       temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.status==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE COPIED SUCCESSFULLY\n");
                       break;
                case 4:scanf("%s",s);
                       scanf("%s",d);
                       send_buf.mtype=1;
                       send_buf.mbody.req = MV;
                       send_buf.mbody.sender = getpid();
                       strcpy(send_buf.mbody.paths[0],s);
                       strcpy(send_buf.mbody.paths[1],d);
                       temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.status==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE MOVED SUCCESSFULLY\n");
                       break;
                case 5:scanf("%s",s); // take in input the desired location in M.
                       send_buf.mtype=1;
                       send_buf.mbody.req = RM;
                       send_buf.mbody.sender = getpid();
                       strcpy(send_buf.mbody.paths[0],s);
                       temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.status==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE REMOVED SUCCESSFULLY\n");
                       break;
                case 6:scanf("%s",cmd);
                       scanf("%s",s);
                       send_buf.mtype=1;
                       send_buf.mbody.req = COMMAND;
                       send_buf.mbody.sender = getpid();
                       strcpy(send_buf.mbody.paths[0],s);
                       temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.status==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE ADDED SUCCESSFULLY\n");
                       break;
                        


            }
    }
}

int main(int argc, char ** argv) {
    client();
    return 0;
}
