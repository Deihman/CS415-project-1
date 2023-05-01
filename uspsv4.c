#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
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
int numprograms = 0;
int numpids;
pid_t pid[MAXPROGRAMS];
int gactiveiter = -1;
char* programs[MAXPROGRAMS];
char* args[MAXPROGRAMS][MAXARGS+1];

/* 
 * two files per program
 * stat: comm (2), utime (14), stime (15), rss (24)
 * io: rchar, wchar
 * 
 * comm is the program name
 * utime + stime / sysconf(_SC_CLK_TCK) is the program runtime
 * rss is the actual amount of memory used
 * rchar is the number of bytes read by the program
 * wchar is the number of bytes written by the program
 */
void printstatus(pid_t p)
{
        char pidstr[BUFFLEN];
        char prefix[BUFFLEN] = "/proc/";
        char pidstat[BUFFLEN];
        char pidio[BUFFLEN];

        char comm[BUFFLEN];
        char utimestr[BUFFLEN];
        char stimestr[BUFFLEN];
        char combtimestr[BUFFLEN];
        char rss[BUFFLEN];

        char rchar[BUFFLEN];
        char wchar[BUFFLEN];

        int utimeint;
        int stimeint;
        int combtime;
        long tick = sysconf(_SC_CLK_TCK);

        p1itoa(p, pidstr);
        p1strcat(prefix, pidstr);
        p1strcpy(pidstat, prefix);
        p1strcpy(pidio, prefix);
        p1strcat(pidstat, "/stat");
        p1strcat(pidio, "/io");

        int statfd = open(pidstat, O_RDONLY);
        int iofd = open(pidio, O_RDONLY);

        if (statfd == -1 || iofd == -1)
        {
                p1putstr(p1stderr, "unable to open proc files\n");
                _exit(1);
        }

        int wordindex = 0;
        int charindex = 0;        

        char linebuffer[BUFFLEN];
        char wordbuffer[BUFFLEN];

        if (p1getline(statfd, linebuffer, sizeof(linebuffer)) == 0)
        {
                p1putstr(p1stderr, "unable to read stat\n");
                _exit(1);
        }

        //p1putstr(p1stdout, linebuffer);

        while ((charindex = p1getword(linebuffer, charindex, wordbuffer)) != -1)
        {
                switch(wordindex)
                {
                        case 1:
                                p1strcpy(comm, wordbuffer);
                                break;
                        case 13:
                                p1strcpy(utimestr, wordbuffer);
                                break;
                        case 14:
                                p1strcpy(stimestr, wordbuffer);
                                break;
                        case 23:
                                p1strcpy(rss, wordbuffer);
                                break;
                        default:
                                break;
                }

                wordindex++;
        }

        if (wordindex < 23)
        {
                p1putstr(p1stderr, "unable to parse stat\n");
                _exit(1);
        }

        if (p1getline(iofd, linebuffer, sizeof(linebuffer)) == 0)
        {
                p1putstr(p1stderr, "unable to read io\n");
                _exit(1);
        }

        /*p1putstr(p1stdout, linebuffer);*/

        charindex = 0;
        while (p1getline(iofd, linebuffer, sizeof(linebuffer)) != 0)
        {
                charindex = 0;
                charindex = p1getword(linebuffer, charindex, wordbuffer);
                if (p1strneq(wordbuffer, "rchar:", 6))
                        charindex = p1getword(linebuffer, charindex, rchar);
                if (p1strneq(wordbuffer, "wchar:", 6))
                        charindex = p1getword(linebuffer, charindex, wchar);
        }

        utimeint = p1atoi(utimestr);
        stimeint = p1atoi(stimestr);

        combtime = (utimeint + stimeint) / tick;

        p1itoa(combtime, combtimestr);

        p1putstr(p1stdout, combtimestr);
        p1putstr(p1stdout, ", ");
        p1putstr(p1stdout, rss);
        p1putstr(p1stdout, ", ");
        /*p1putstr(p1stdout, rchar);
        p1putstr(p1stdout, ", ");*/
        p1putstr(p1stdout, wchar);
        p1putstr(p1stdout, ", ");
        p1putstr(p1stdout, comm);
        p1putstr(p1stdout, "\n");

        close(statfd);
        close(iofd);
}


void cleanup()
{
        /* closers */
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


void handler(int)
{
        if (execvp(gp, ga) == -1)
        {
                p1putstr(p1stderr, "execvp failed\n");
                _exit(1);
        }
}


void alarmhandler(int)
{
        if (numpids == 0)
        {
                cleanup();
                _exit(0);
        }
        if (gactiveiter != -1)
                kill(pid[gactiveiter], SIGSTOP);
        gactiveiter = (gactiveiter + 1) % numpids;

        kill(pid[gactiveiter], SIGCONT);

        p1putstr(p1stdout, "CPUT\tMem\tIO w\tCmd\n");
        for (int i = 0; i < numpids; i++)
                printstatus(pid[i]);
}


void removeandshift(pid_t id)
{
        int i;
        for (i = 0; i < numpids && pid[i] != id; i++)
                ;
        if (i == numpids)
                return;
        
        for (int j = i; j < numpids-1; j++)
        {
                pid[j] = pid[j+1];
        }
        numpids--;
}


void deadchildhandler(int)
{
        pid_t p;

        while ((p = waitpid(-1, NULL, WNOHANG)) > 0)
        {
                if (numpids == 0)
                {
                        cleanup();
                        _exit(0);
                }

                removeandshift(p);
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
                free(filename);
                if (fd == -1)
                {
                        p1putstr(p1stderr, "Unable to open workload file\n");
                        _exit(1);
                }
        }
        /* 
           build arg structure: store functions in char**, args for each function in char***
        */
        char buffer[BUFFLEN];
        char word[BUFFLEN];

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
                        p1putstr(p1stderr, "WARNING: argument buffer is full, continuing\n");

                if (yeah)
                        args[numprograms++][numargs] = NULL;
        }
        if (fd != p1stdin)
                close(fd);

        numpids = numprograms;

        if (numprograms == MAXPROGRAMS)
                p1putstr(p1stderr, "WARNING: program buffer is full, continuing\n");

        p1putstr(p1stdout, "Starting forks\n");

        for (int i = 0; i < numprograms; i++)
        {
                pid[i] = fork();
        
                if (pid[i] == 0)
                {
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

        if (signal(SIGALRM, alarmhandler) == SIG_ERR)
        {
                p1perror(p1stderr, "signal error\n");
                _exit(1);
        }

        if (signal(SIGCHLD, deadchildhandler) == SIG_ERR)
        {
                p1perror(p1stderr, "signal error\n");
                _exit(1);
        }

        p1putstr(p1stdout, "waiting for children (5 sec)\n");
        sleep(5);
        for (int i = 0; i < numprograms; i++)
        {
               kill(pid[i], SIGUSR1);
               kill(pid[i], SIGSTOP);
        }

        struct itimerval it;
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = quantum * 1000;
        it.it_value.tv_sec = 0;
        it.it_value.tv_usec = quantum* 1000;
        setitimer(ITIMER_REAL, &it, NULL);

        while (1)
        { pause(); }
}