#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "p1fxns.h"

/* 
   4096 length buffer
   256 possible programs with 256 characters 
   256 arguments for each program
*/
#define BUFFLEN 4096
#define MAXPROGRAMS 256
#define MAXARGS 256

void main(int argc, char** argv)
{
        extern char* optarg;
        extern int optind;
        enum {stdin = 0, stdout = 1, stderr = 2};

        int opt;
        int q = 0;
        int quantum = -1;
        char* filename = NULL;
        int checkstdin = 0;
        int fd = stdin;

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
                                p1putstr(stderr, "usage: ./uspsv? [-q <quantum in msec>] [workload_file]\n");
                                _exit(1);
                                break;
                }
        }

        if (quantum == -1)
        {
                p1putstr(stderr, "Invalid quantum value\n");
                _exit(1);
        }

        if (optind < argc)
        {
                filename = p1strdup(argv[optind++]);
        }

        if (optind < argc)
        {
                p1putstr(stderr, "usage: ./uspsv? [-q <quantum in msec>] [workload_file]\n");
                _exit(1);
        }

        if (filename != NULL)
        {
                fd = open(filename, O_RDONLY);
                if (fd == -1)
                {
                        p1putstr(stderr, "Unable to open workload file\n");
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
        char* args[MAXPROGRAMS][MAXARGS];
        int numprograms = 0;

        while (numprograms < MAXPROGRAMS && p1getline(fd, buffer, sizeof(buffer)) != 0)
        {
                int yeah = 0;
                int getworditer = 0;
                int numargs = 0; /* per word */
                while (numargs < MAXARGS && (getworditer = p1getword(buffer, getworditer, word)) != -1)
                {
                        yeah = 1;
                        if (numargs == 0)
                                programs[numprograms] = p1strdup(word);
                        args[numprograms][numargs++] = p1strdup(word);
                }

                if (numargs == MAXARGS)
                        p1putstr(stderr, "WARNING: argument buffer is full, continuing\n");

                if (yeah)
                        args[numprograms++][numargs] = NULL;
        }

        if (numprograms == MAXPROGRAMS)
                p1putstr(stderr, "WARNING: program buffer is full, continuing\n");

        p1putstr(stdout, "Starting forks\n");

        pid_t pid[MAXPROGRAMS];
        for (int i = 0; i < numprograms; i++)
        {
                pid[i] = fork();
                if (pid[i] == 0)
                {
                        if (execvp(programs[i], args[i]) == -1)
                        {
                                p1putstr(stderr, "execvp error\n");
                                _exit(1);
                        }
                        break;
                }
        }

        for (int i = 0; i < numprograms; i++)
               wait(&pid[i]);

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
}