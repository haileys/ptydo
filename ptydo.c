#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>
#include <signal.h>
#include <sys/select.h>

/*
void usage()
{
    printf("Usage: ptydo [<switches>] [--] <command>\n");
    printf("  -r    rows\n");
    printf("  -c    columns\n");
}
*/

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    struct winsize window;
    window.ws_row = 25;
    window.ws_col = 80;
    
    int ptyfd;
    pid_t child = forkpty(&ptyfd, NULL, NULL, &window);
    
    if(child == -1) {
        // forkpty failed:
        perror("forkpty");
        exit(EXIT_FAILURE);
    }
    
    if(child == 0) {
        // we are the child process
//        execvp(argv[1], argv + 1);
        // execvp failed:
        char* ptyargv[] = { "bash", NULL };
        execv("/bin/bash", ptyargv);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    
    while(1) {
        fd_set readfds;
        fd_set errfds;
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);
        FD_SET(ptyfd, &readfds);
        FD_ZERO(&errfds);
        FD_SET(0, &errfds);
        FD_SET(ptyfd, &errfds);
        int err = select(ptyfd + 1, &readfds, NULL, &errfds, NULL);
        if(err < 0) {
            perror("select");
        }
        int readfd, writefd;
        if(FD_ISSET(0, &readfds)) {
            // data on stdin
            readfd = 0;
            writefd = ptyfd;
        } else if(FD_ISSET(ptyfd, &readfds)) {
            writefd = 0;
            readfd = ptyfd;
        } else if(FD_ISSET(0, &errfds) || FD_ISSET(0, &readfds)) {
            kill(child, SIGTERM);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "this should never happen\n");
            exit(EXIT_FAILURE);
        }
        char buff[1024];
        ssize_t bytes = read(readfd, buff, sizeof(buff));
        if(bytes == 0) {
            kill(child, SIGTERM);
            exit(EXIT_SUCCESS);
        } else if(bytes < 0) {
            perror("read");
        }
        write(writefd, buff, bytes);
    }
    
    return 0;
}