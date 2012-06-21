#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#ifdef __linux__
    #include <pty.h>
#elif __APPLE__
    #include <util.h>
#else
    #error Not sure which header forkpty() is in on your system. Please report a bug at https://github.com/charliesome/ptydo/issues
#endif
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>

static void usage()
{
    printf(
        "Usage: ptydo [<switches>] [--] <command...>\n"
        "\n"
        "Runs command in a child PTY with input/output exposed as stdin/stdout.\n"
        "\n"
        "Window Options:\n"
        "  -w <columns>  Specifies the width of the PTY in columns\n"
        "  -h <rows>     Specifies the height of the PTY in rows\n"
        "\n"
        "  If ptydo is run interactively, the PTY will default to the dimensions of the\n"
        "  host PTY. Otherwise, the dimensions will default to 80 by 25.\n"
        "\n"
        "Misc Options:\n"
        "  -c            Forward SIGINT to PTY.\n"
    );
    exit(EXIT_SUCCESS);
}

static pid_t child;
static int ptyfd;

static void exit_and_kill_child(int status) {
    if(child) {
        kill(child, SIGTERM);
    }
    exit(status);
}

static void forward_signal_to_pty(int sig) {
    kill(child, sig);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    struct winsize window;
    window.ws_row = 25;
    window.ws_col = 80;
    
    if(isatty(0)) {
        ioctl(0, TIOCGWINSZ, &window);
    }
    
    if(argc < 2) {
        usage();
    }
    
    int argi;
    for(argi = 1; argi < argc; argi++) {
        if(argv[argi][0] != '-') {
            break;
        }
        if(strlen(argv[argi]) > 2) {
            printf("Unknown option '%s'\n", argv[argi]);
            exit(EXIT_FAILURE);
        }
        if(argv[argi][1] == '-') {
            argi++;
            break;
        }
        switch(argv[argi][1]) {
            case 'w':
                if(argi + 1 == argc) {
                    printf("Expected PTY width after -w flag");
                    exit(EXIT_FAILURE);
                }
                window.ws_col = atoi(argv[++argi]);
                break;
            case 'h':
                if(argi + 1 == argc) {
                    printf("Expected PTY height after -h flag");
                    exit(EXIT_FAILURE);
                }
                window.ws_row = atoi(argv[++argi]);
                break;
            case 'c':
                signal(SIGINT, forward_signal_to_pty);
                break;
            default:
                printf("Unknown option '-%c'\n", argv[argi][1]);
                exit(EXIT_FAILURE);
        }
    }
    
    if(argi == argc) {
        printf("No command specified\n");
        exit(EXIT_FAILURE);
    }
    
    child = forkpty(&ptyfd, NULL, NULL, &window);
    
    if(child == -1) {
        // forkpty failed:
        perror("forkpty");
        exit(EXIT_FAILURE);
    }
    
    if(child == 0) {
        // we are the child process
//        execvp(argv[1], argv + 1);
        // execvp failed:
        char* ptyargv[argc - argi + 1];
        memcpy(ptyargv, argv + argi, (argc - argi) * sizeof(char*));
        ptyargv[argc - argi] = NULL;
        execvp(ptyargv[0], ptyargv);
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
            if(errno == EINTR) {
                continue;
            }
            perror("select");
        }
        
        int readfd, writefd;
        if(FD_ISSET(0, &readfds)) {
            // data on stdin
            readfd = 0;
            writefd = ptyfd;
        } else if(FD_ISSET(ptyfd, &readfds)) {
            writefd = 1;
            readfd = ptyfd;
        } else if(FD_ISSET(0, &errfds) || FD_ISSET(ptyfd, &readfds)) {
            exit_and_kill_child(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "this should never happen\n");
            exit_and_kill_child(EXIT_FAILURE);
        }
        
        char buff[1024];
        ssize_t bytes = read(readfd, buff, sizeof(buff));
        if(bytes == 0) {
            exit_and_kill_child(EXIT_SUCCESS);
        } else if(bytes < 0) {
            perror("read");
            exit_and_kill_child(EXIT_FAILURE);
        }
        write(writefd, buff, bytes);
    }
    
    return 0;
}
