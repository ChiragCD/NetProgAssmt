#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#define STRSIZE 200

void tee (int num_pipes, int in, int out1, int out2, int out3);

void run_simple(char * command) {
    //perror(command);
    tee(1, 0, 1, 0, 0);
}

void read_in_file (char * command, char * in_file) {
    char * temp = in_file;
    in_file[0] = '\0';
    while (*command != '<')
    {
        if(*command == '|' || *command == ',' || *command == '\0') return;
        else command++;
    }
    command++;
    while(*command == ' ') command++;
    while(*command != ' ' && *command != '|' && *command != '\0' && *command != '<' && *command != '>' && *command != ',') *(temp++) = *(command++);
    *temp = '\0';
    return;
}

void read_out_file (char * command, char * out_file) {
    char * temp = out_file;
    out_file[0] = '\0';
    while (*command != '>')
    {
        if(*command == '|' || *command == ',' || *command == '\0') return;
        else command++;
    }
    command++;
    while(*command == ' ') command++;
    while(*command != ' ' && *command != '|' && *command != '\0' && *command != '<' && *command != '>' && *command != ',') *(temp++) = *(command++);
    *temp = '\0';
    return;
}

void read_sub_command (char * command, char * sub_command) {
    char * temp = sub_command;
    sub_command[0] = '\0';
    while(*command != '<' && *command != '>' && *command != '|' && *command != '\0' && *command != ',') *(temp++) = *(command++);
    *temp = '\0';
    return;
}

void read_sub_command_with_redirect (char * command, char * sub_command) {
    char * temp = sub_command;
    sub_command[0] = '\0';
    while(*command != '|' && *command != '\0' && *command != ',') *(temp++) = *(command++);
    *temp = '\0';
    return;
}

void tee (int num_pipes, int in, int out1, int out2, int out3) {
    if(num_pipes == 0) return;
    fprintf(stderr, "%d - Running internal tee process with %d outputs\n", getpid(), num_pipes);
    char buf[1024];
    int outs[3] = {out1, out2, out3};
    int read_amt = read(in, buf, sizeof(buf));
    while(read_amt > 0) {
        for(int i = 0; i < num_pipes; i++) {
            //printf("Tee transferring %s\n", buf);
            write(outs[i], buf, read_amt);
            /*
            int written = 0;
            while(written != read_amt) {
                written += write(outs[i], buf, )
            }
            */
        }
        read_amt = read(in, buf, sizeof(buf));
    }
    for(int i = 0; i < num_pipes; i++) close(outs[i]);
    close(in);
    fprintf(stderr, "%d - Ending internal tee process with %d outputs\n", getpid(), num_pipes);
    exit(0);
}

void setup_pipes(int num_pipes, int ** pipes) {
    for(int i = 0; i < num_pipes; i++) if(pipe(pipes[i]) == -1) perror("Pipe creation error\n");
}

int setup_forks(int num_pipes) {
    for(int i = num_pipes; i > 0; i--) {
        int is_parent = fork();
        if(is_parent == -1) {
            perror("Fork error ");
        }
        if(!is_parent) return i;
        else fprintf(stderr, "%d - Forked to handle subcommand(s), child id %d\n", getpid(), is_parent);
    }
    return 0;
}

void execute (char * command) {

    int status;

    if(!*command) exit(0);

    char * parent_command = (char *) malloc(sizeof(char) * STRSIZE);
    char * in_file = (char *) malloc(sizeof(char) * STRSIZE);
    char * out_file = (char *) malloc(sizeof(char) * STRSIZE);

    read_in_file(command, in_file);
    read_out_file(command, out_file);
    read_sub_command(command, parent_command);

    while(*command != '\0' && *command != '|') command++;

    int num_pipes = 0;
    while(*command == '|') {
        num_pipes++;
        command++;
    }

    if(*in_file) {
        int in = open(in_file, O_RDONLY);
        dup2(in, 0);
    }
    if(*out_file) {
        int out = open(out_file, O_CREAT|O_WRONLY, 0666);
        dup2(out, 1);
    }

    if(!num_pipes) {
        fprintf(stderr, "%d - Running non-piped command %s\n", getpid(), parent_command);
        run_simple(parent_command);
        fprintf(stderr, "%d - Finished running %s\n", getpid(), parent_command);
        exit(0);
    }

    if(num_pipes == 1) {
        fprintf(stderr, "%d - Encountered | command from %s to %s\n", getpid(), parent_command, command);
        int pipe_ends[2];
        int err = pipe(pipe_ends);
        if(err) {
            perror("Error ");
        }
        int self_num = setup_forks(num_pipes);

        if(self_num == 0) {
            dup2(pipe_ends[1], 1);
            close(pipe_ends[0]);
            execute(parent_command);
            close(pipe_ends[1]);
            wait(&status);
            fprintf(stderr, "%d - Finished running %s\n", getpid(), parent_command);
            exit(0);
        }
        else {
            dup2(pipe_ends[0], 0);
            close(pipe_ends[1]);
            execute(command);
            close(pipe_ends[0]);
            exit(0);
        }
    }

    char * sub_command1 = (char *) malloc(sizeof(char) * STRSIZE);
    read_sub_command_with_redirect(command, sub_command1);
    while(*command != '\0' && *command != ',') command++;
    if(*command == ',') command++;

    if(num_pipes == 2){
        fprintf(stderr, "%d - Encountered || command from %s to %s and %s\n", getpid(), parent_command, sub_command1, command);
        int pipe1_ends[2];
        int pipe2_ends[2];
        int tee_pipe[2];
        int err = pipe(pipe1_ends);
        if(err) {
            perror("Error ");
        }
        err = pipe(pipe2_ends);
        if(err) {
            perror("Error ");
        }
        int self_num = setup_forks(num_pipes);

        if(self_num == 2) {
            dup2(pipe1_ends[0], 0);
            close(pipe1_ends[1]);
            close(pipe2_ends[0]);
            close(pipe2_ends[1]);
            execute(command);
            close(pipe1_ends[0]);
            exit(0);
        }
        if(self_num == 1) {
            dup2(pipe2_ends[0], 0);
            close(pipe2_ends[1]);
            close(pipe1_ends[0]);
            close(pipe1_ends[1]);
            execute(sub_command1);
            close(pipe2_ends[0]);
            exit(0);
        }

        pipe(tee_pipe);
        if(err) {
            perror("Pipe creation error ");
        }
        int tee_proc = fork();
        if(tee_proc == -1) {
            perror("Fork error ");
        }
        if(tee_proc == 0) {
            close(tee_pipe[1]);
            close(pipe1_ends[0]);
            close(pipe2_ends[0]);
            tee(num_pipes, tee_pipe[0], pipe1_ends[1], pipe2_ends[1], 0);
            exit(0);
        }

        close(pipe1_ends[0]);
        close(pipe2_ends[0]);
        close(pipe1_ends[1]);
        close(pipe2_ends[1]);
        close(tee_pipe[0]);

        if(self_num == 0) {
            dup2(tee_pipe[1], 1);
            execute(parent_command);
            close(tee_pipe[1]);
            wait(&status);
            wait(&status);
            wait(&status);
            fprintf(stderr, "%d - Finished running %s\n", getpid(), parent_command);
            exit(0);
        }
    }

    char * sub_command2 = (char *) malloc(sizeof(char) * STRSIZE);
    read_sub_command_with_redirect(command, sub_command2);
    while(*command != '\0' && *command != ',') command++;
    if(*command == ',') command++;

    if(num_pipes == 3) {
        fprintf(stderr, "%d - Encountered ||| command from %s to %s, %s and %s\n", getpid(), parent_command, sub_command1, sub_command2, command);
        int pipe1_ends[2];
        int pipe2_ends[2];
        int pipe3_ends[2];
        int tee_pipe[2];
        int err = pipe(pipe1_ends);
        if(err) {
            perror("Pipe creation error ");
        }
        err = pipe(pipe2_ends);
        if(err) {
            perror("Pipe creation error ");
        }
        err = pipe(pipe3_ends);
        if(err) {
            perror("Pipe creation error ");
        }
        int self_num = setup_forks(num_pipes);

        if(self_num == 3) {
            dup2(pipe3_ends[0], 0);
            close(pipe1_ends[0]);
            close(pipe1_ends[1]);
            close(pipe2_ends[0]);
            close(pipe2_ends[1]);
            close(pipe3_ends[1]);
            execute(command);
            close(pipe1_ends[0]);
            exit(0);
        }
        if(self_num == 2) {
            dup2(pipe1_ends[0], 0);
            close(pipe1_ends[1]);
            close(pipe2_ends[0]);
            close(pipe2_ends[1]);
            close(pipe3_ends[0]);
            close(pipe3_ends[1]);
            execute(sub_command2);
            close(pipe1_ends[0]);
            exit(0);
        }
        if(self_num == 1) {
            dup2(pipe2_ends[0], 0);
            close(pipe2_ends[1]);
            close(pipe1_ends[0]);
            close(pipe1_ends[1]);
            close(pipe3_ends[0]);
            close(pipe3_ends[1]);
            execute(sub_command1);
            close(pipe2_ends[0]);
            exit(0);
        }

        pipe(tee_pipe);
        if(err) {
            perror("Pipe creation error ");
        }
        int tee_proc = fork();
        if(tee_proc == -1) {
            perror("Fork error ");
        }
        if(tee_proc == 0) {
            close(tee_pipe[1]);
            close(pipe1_ends[0]);
            close(pipe2_ends[0]);
            close(pipe3_ends[0]);
            tee(num_pipes, tee_pipe[0], pipe1_ends[1], pipe2_ends[1], pipe3_ends[1]);
            exit(0);
        }

        close(pipe1_ends[0]);
        close(pipe2_ends[0]);
        close(pipe1_ends[1]);
        close(pipe2_ends[1]);
        close(pipe3_ends[0]);
        close(pipe3_ends[1]);
        close(tee_pipe[0]);

        if(self_num == 0) {
            dup2(tee_pipe[1], 1);
            execute(parent_command);
            close(tee_pipe[1]);
            wait(&status);
            wait(&status);
            wait(&status);
            wait(&status);
            fprintf(stderr, "%d - Finished running %s\n", getpid(), parent_command);
            exit(0);
        }
    }
}

void shell(int argc, char ** argv) {
    while(1) {
        int status;
        //accept();
        execute("ls|wc|wc|wc");
        wait(&status);
        break;
    }
    return;
}

int main (int argc, char ** argv) {
    shell(argc, argv);
    return 0;
}