#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "p1fxns.h"

void main(int argc, char** argv)
{
        extern char* optarg;
        extern int optind;
        enum {stdin = 0, stdout = 1, stderr = 2};
        int env = setenv("USPS_QUANTUM_MSEC", "500", 1);

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
        char buffer[256];
        char word[256];

        /* 
           256 possible programs with 256 characters 
           256 arguments for each program
        */
        char* programs[256];
        char* args[256][256];
        int numprograms = 0;

        int outeriter = 0; /* per line */
        while (p1getline(fd, buffer, sizeof(buffer)) != 0)
        {
                int i = 0;
                int inneriter = 0; /* per word */
                while ((i = p1getword(buffer, i, word)) != -1)
                {
                        if (inneriter == 0)
                                programs[outeriter] = word;
                        else
                                args[outeriter][inneriter - 1] = word;
                        
                        inneriter++;
                }
                args[outeriter][inneriter - 1] = NULL;
                outeriter++;
                numprograms++;
        }

        int pid[numprograms];
        for (int i = 0; i < numprograms; i++)
        {
                pid[i] = fork();
                if (pid[i] = 0)
                {
                        execvp(programs[i], args[i]);
                }
        }

        for (int i = 0; i < numprograms; i++)
        {
                wait(&pid[i]);        
        }

        /* closers */
        if (filename != NULL)
        {
                free(filename);
                close(fd);
        }
}