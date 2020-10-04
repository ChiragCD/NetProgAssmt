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
    for(;;){
            printf("Enter the type of operation you would like to perform:\n
                            1. ADD FILE <file path>\n
                            2. ADD CHUNK <file path> <machine file path> <chunk number>\n
                            3. COPY <source> <destination>\n
                            4. MOVE <source> <destination>\n
                            5. REMOVE <file path>\n
                            6. COMMAND <command> <chunk name>\n");
            int choice;
            choice = getchar();
            choice -= '0';
            char cmd[100],s[100],d[100],fp[100];
            switch(choice){
                case 1:scanf("%s",s); // take in input the desired location in M.
                       snd_buf.mtype=1;
                       snd_buf.mbody.req = ADD_FILE;
                       snd_buf.mbody.sender = getpid();
                       strcpy(snd_buf.mbody.paths[0],s);
                       ssize_t temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.error==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE ADDED SUCCESSFULLY\n");
                       break;
                case 2:scanf("%s",s); 
                       scanf("%s",fp); // get the local file path inside client file-directory
                       snd_buf.mtype=1;
                       snd_buf.mbody.req = ADD_CHUNK;
                       snd_buf.mbody.sender = getpid();
                       strcpy(snd_buf.mbody.paths[0],s);
                       int fd;
                       if(fd = open(fp,O_RDONLY) == -1){
                               printf("Could not find file with name %s.\n",fp);
                               close(fd);
                       }
                       ssize_t temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.error==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE ADDED SUCCESSFULLY\n");
                       break;
                       break;
                case 3:scanf("%s",s);
                       scanf("%s",d);
                       snd_buf.mtype=1;
                       snd_buf.mbody.req = CP;
                       snd_buf.mbody.sender = getpid();
                       strcpy(snd_buf.mbody.paths[0],s);
                       strcpy(snd_buf.mbody.paths[1],d);
                       ssize_t temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.error==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE COPIED SUCCESSFULLY\n");
                       break;
                case 4:scanf("%s",s);
                       scanf("%s",d);
                       snd_buf.mtype=1;
                       snd_buf.mbody.req = MV;
                       snd_buf.mbody.sender = getpid();
                       strcpy(snd_buf.mbody.paths[0],s);
                       strcpy(snd_buf.mbody.paths[1],d);
                       ssize_t temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.error==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE MOVED SUCCESSFULLY\n");
                       break;
                case 5:scanf("%s",s); // take in input the desired location in M.
                       snd_buf.mtype=1;
                       snd_buf.mbody.req = RM;
                       snd_buf.mbody.sender = getpid();
                       strcpy(snd_buf.mbody.paths[0],s);
                       ssize_t temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.error==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE REMOVED SUCCESSFULLY\n");
                       break;
                case 6:scanf("%s",cmd);
                       scanf("%s",s);
                       snd_buf.mtype=1;
                       snd_buf.mbody.req = COMMAND;
                       snd_buf.mbody.sender = getpid();
                       strcpy(snd_buf.mbody.paths[0],s);
                       ssize_t temp = msgsnd(mqid,&send_buf,MSGSIZE,0);
                       ssize_t msgsize = msgrcv(mqid, &recv_buf, MSGSIZE, getpid(), 0);
                       if(recv_buf.mbody.error==-1)
                               printf("%s\n",recv_buf.mbody.error);
                       else 
                               printf("FILE ADDED SUCCESSFULLY\n");
                       break;
                        


            }

int main(int argc, char ** argv) {
    client();
    return 0;
}
