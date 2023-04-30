#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "p1fxns.h"

/* 
   4096 length buffer
   256 possible programs with 256 characters 
   256 arguments for each program
*/
#define BUFFLEN 4096
#define MAXPROGRAMS 256
#define MAXARGS 256

enum {p1stdin = 0, p1stdout = 1, p1stderr = 2};
char* gp;
char** ga;
int gnp;
pid_t pid[MAXPROGRAMS];

void handler(int)
{
        printf("child %d exec %s\n", getpid(), gp);
        if (execvp(gp, ga) == -1)
        {
                p1putstr(p1stderr, "execvp failed\n");
                _exit(1);
        }
}


void main(int argc, char** argv)
{
        extern char* optarg;
        extern int optind;

        int opt;
        int q = 0;
        int quantum = -1;
        char* filename = NULL;
        int checkstdin = 0;
        int fd = p1stdin;

        char* p;
        if ((p = getenv("USPS_QUANTUM_MSEC")) != NULL)
                quantum = p1atoi(p);

        while((opt = getopt(argc, argv, "q:")) != -1)
        {
                switch (opt)
                {
                        case 'q':
                                quantum = p1atoi(optarg);
                                q = 1;
                                break;

                        case '?':
                                p1putstr(p1stderr, "usage: ./uspsv? [-q <quantum in msec>] [workload_file]\n");
                                _exit(1);
                                break;
                }
        }

        if (quantum == -1)
        {
                p1putstr(p1stderr, "Invalid quantum value\n");
                _exit(1);
        }

        if (optind < argc)
        {
                filename = p1strdup(argv[optind++]);
        }

        if (optind < argc)
        {
                p1putstr(p1stderr, "usage: ./uspsv? [-q <quantum in msec>] [workload_file]\n");
                _exit(1);
        }

        if (filename != NULL)
        {
                fd = open(filename, O_RDONLY);
                if (fd == -1)
                {
                        p1putstr(p1stderr, "Unable to open workload file\n");
                        free(filename);
                        _exit(1);
                }
        }
        /* 
           build arg structure: store functions in char**, args for each function in char***
        */
        char buffer[BUFFLEN];
        char word[BUFFLEN];

        char* programs[MAXPROGRAMS];
        char* args[MAXPROGRAMS][MAXARGS+1];
        int numprograms = 0;

        while (numprograms < MAXPROGRAMS && p1getline(fd, buffer, sizeof(buffer)) != 0)
        {
                int yeah = 0;
                int getworditer = 0;
                int numargs = 0; /* per word */
                printf("buffer: %s\n", buffer);
                while (numargs < MAXARGS && (getworditer = p1getword(buffer, getworditer, word)) != -1)
                {
                        yeah = 1;
                        printf("word: %s\n", word);
                        if (numargs == 0)
                                programs[numprograms] = p1strdup(word);
                        args[numprograms][numargs++] = p1strdup(word);
                }

                if (numargs == MAXARGS)
                        p1putstr(p1stderr, "WARNING: argument buffer is full, continuing\n");

                if (yeah)
                        args[numprograms++][numargs] = NULL;
        }

        gnp = numprograms;

        if (numprograms == MAXPROGRAMS)
                p1putstr(p1stderr, "WARNING: program buffer is full, continuing\n");

        printf("numprograms: %d\n", numprograms);
        p1putstr(p1stdout, "Starting forks\n");

        for (int i = 0; i < numprograms; i++)
        {
                pid[i] = fork();
                if (pid[i] == 0)
                        printf("Child process %d\n", getpid());
                else
                        printf("pid %d forked to %d\n", getpid(), pid[i]);

                if (pid[i] == 0)
                {
                        printf("child %d waiting\n", getpid());
                        gp = programs[i];
                        ga = args[i];
                        if (signal(SIGUSR1, handler) == SIG_ERR)
                        {
                                p1perror(p1stderr, "signal error\n");
                                _exit(1);
                        }
                        sleep(500);
                }
        }

        printf("Hello from pid %d\n", getpid());

        p1putstr(p1stdout, "waiting for children (5 sec)\n");
        sleep(5);
        for (int i = 0; i < numprograms; i++)
        {
               printf("pid %d sending SIGUSR1 to pid %d\n", getpid(), pid[i]);
               kill(pid[i], SIGUSR1);
        }

        for (int i = 0; i < numprograms; i++)
        {
               printf("pid %d sending SIGSTOP to pid %d\n", getpid(), pid[i]);
               kill(pid[i], SIGSTOP);
        }

        for (int i = 0; i < numprograms; i++)
        {
               printf("pid %d sending SIGCONT to pid %d\n", getpid(), pid[i]);
               kill(pid[i], SIGCONT);
        }

        for (int i = 0; i < numprograms; i++)
        {
               printf("pid %d waiting on pid %d\n", getpid(), pid[i]);
               wait(&pid[i]);
        }

        /* closers */
        if (filename != NULL)
        {
                free(filename);
                close(fd);
        }

        for(int i = 0; i < numprograms; i++)
        {
                free(programs[i]);
                for (int j = 0; args[i][j] != NULL; j++)
                {
                        free(args[i][j]);
                }
        }

        p1putstr(p1stdout, "exiting...\n");
}