/*
 * Author: Aidan Fernandez
 * Clemson Login Name: Aferna6
 * Course Number: ECE 2220
 * Semester: Spring 2025
 * Assignment Number: Lab 7 – Signals and Threads
 *
 * Purpose:
 *   This program simulates a gardener taking care of three plant processes.
 *   The parent process spawns three child processes (plants) that consume
 *   resources, grow, and eventually either die or are sold. The gardener can
 *   feed, water, check status, or quit.
 *
 * Status Tracking:
 *   - Each plant child exits with code 1 if sold, 2 if died.
 *   - Parent SIGCHLD handler captures exit code and records in plant_state[].
 *   - status 's' prints: "Alive", "Sold", or "Dead" accordingly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define NUM_PLANTS 3
#define MAX_TTYS 4
#define MAX_PTS 256

int tty_fds[MAX_TTYS];
pid_t plant_pids[NUM_PLANTS];
// 0=alive,1=sold,2=died
int plant_state[NUM_PLANTS] = {0,0,0};

volatile int fertilizer_mg, water_ml, growth_rows, child_idx;
int out_fd = -1;

// prototypes
typedef void (*sighandler_t)(int);
void open_terminals();
void spawn_plants();
void parent_loop();
void sigchld_handler(int);
void plant_loop(int idx);
void handle_feed(int);
void handle_water(int);
void print_initial(int idx);
void print_growth(int idx,int rows);
int cmp_int(const void *a,const void *b);

int main() {
    open_terminals();

    struct sigaction sa = {0};
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction"); exit(EXIT_FAILURE);
    }

    spawn_plants();
    parent_loop();
    return 0;
}

int cmp_int(const void *a,const void *b) {
    return (*(int*)a - *(int*)b);
}

void open_terminals() {
    DIR *dir = opendir("/dev/pts");
    if (!dir) { perror("opendir"); exit(EXIT_FAILURE); }
    int minors[MAX_PTS], count=0; struct dirent *d;
    while ((d=readdir(dir))) {
        if (!isdigit(d->d_name[0])) continue;
        int m = atoi(d->d_name);
        char path[32]; snprintf(path,sizeof(path),"/dev/pts/%d",m);
        int fd = open(path,O_RDONLY);
        if (fd>=0) { close(fd); minors[count++]=m; }
        if (count>=MAX_TTYS) break;
    }
    closedir(dir);
    if (count<MAX_TTYS) {
        fprintf(stderr,"Warning: found %d pts, expected %d\n",count,MAX_TTYS);
    }
    qsort(minors,count,sizeof(int),cmp_int);
    for(int i=0;i<count;i++){
        char path[32]; snprintf(path,sizeof(path),"/dev/pts/%d",minors[i]);
        tty_fds[i]=open(path,O_WRONLY);
        if(tty_fds[i]<0){perror(path);exit(EXIT_FAILURE);}    }
    // extras to parent
    for(int i=count;i<MAX_TTYS;i++) tty_fds[i]=tty_fds[0];
}

void spawn_plants() {
    for(int i=1;i<=NUM_PLANTS;i++) {
        pid_t pid=fork();
        if(pid<0){perror("fork");exit(EXIT_FAILURE);}        
        if(pid==0) {
            child_idx=i; out_fd=tty_fds[i]; plant_loop(i);
            _exit(0);
        }
        plant_pids[i-1]=pid;
    }
}

void parent_loop() {
    out_fd=tty_fds[0];
    char ts[64]; time_t now=time(NULL);
    strftime(ts,sizeof(ts),"Started: %Y-%m-%d %H:%M:%S\n",localtime(&now));
    dprintf(out_fd,"%s",ts);
    char cmd[32];
    while(1) {
        dprintf(out_fd,"Enter command (f<n>, w<n>, s, q): ");
        if(!fgets(cmd,sizeof(cmd),stdin)) continue;
        cmd[strcspn(cmd,"\n")]='\0';
        if(strcmp(cmd,"q")==0) {
            dprintf(out_fd,"Quitting...\n");
            for(int i=0;i<NUM_PLANTS;i++){
                if(plant_state[i]==0) kill(plant_pids[i],SIGTERM);
                waitpid(plant_pids[i],NULL,0);
            }
            break;
        } else if(strcmp(cmd,"s")==0) {
            // print status
            dprintf(out_fd,"Plant Status:\n");
            for(int i=0;i<NUM_PLANTS;i++){
                const char *st;
                if(plant_state[i]==0) st="Alive";
                else if(plant_state[i]==1) st="Sold";
                else st="Dead";
                dprintf(out_fd,"  Plant %d: PID=%d  %s\n", i+1, plant_pids[i], st);
            }
        } else if((cmd[0]=='f'||cmd[0]=='w') && isdigit(cmd[1])) {
            int n=cmd[1]-'0';
            if(n<1||n>NUM_PLANTS){dprintf(out_fd,"Error: bad plant %d\n",n);continue;}
            if(plant_state[n-1]!=0){dprintf(out_fd,"Plant %d not running\n",n);continue;}
            kill(plant_pids[n-1], cmd[0]=='f'?SIGUSR1:SIGUSR2);
        } else {
            dprintf(out_fd,"Invalid command. Use f1..f3, w1..w3, s, q\n");
        }
    }
}

void sigchld_handler(int sig) {
    (void)sig;
    int st; pid_t pid;
    while((pid=waitpid(-1,&st,WNOHANG))>0) {
        for(int i=0;i<NUM_PLANTS;i++){
            if(plant_pids[i]==pid) {
                if(WIFEXITED(st)) plant_state[i]=WEXITSTATUS(st)==0?2:WEXITSTATUS(st);
                break;
            }
        }
    }
}

void plant_loop(int idx) {
    srand(time(NULL)^(getpid()<<16));
    fertilizer_mg=10000; water_ml=1000; growth_rows=0;
    signal(SIGUSR1,handle_feed); signal(SIGUSR2,handle_water);
    print_initial(idx);
    int sec=0; time_t dt=0; int dying=0;
    while(1) {
        int df=(rand()%2001)+1000, dw=(rand()%201)+100;
        fertilizer_mg-=df; water_ml-=dw;
        if(fertilizer_mg<5000) dprintf(out_fd,"Plant %d low fert\n",idx);
        if(water_ml<500)      dprintf(out_fd,"Plant %d low water\n",idx);
        if(fertilizer_mg<=0||water_ml<=0) {
            if(!dying){dying=1; dt=time(NULL);} 
            else if(time(NULL)-dt>=5){
                dprintf(out_fd,"Plant %d died\n",idx);
                _exit(2);
            }
        } else dying=0;
        sleep(1); if(++sec%5==0) {
            growth_rows++; print_growth(idx,growth_rows);
            if(growth_rows>=10){
                dprintf(out_fd,"Plant %d sold\n",idx);
                _exit(1);
            }
        }
    }
}

void handle_feed(int sig) {
    (void)sig; fertilizer_mg+=10000;
    if(fertilizer_mg>10000) fertilizer_mg=10000;
    dprintf(out_fd,"Plant %d: %d mg fert, %d mL water\n",
            child_idx,fertilizer_mg,water_ml);
}

void handle_water(int sig) {
    (void)sig; water_ml+=1000;
    if(water_ml>1000) water_ml=1000;
    dprintf(out_fd,"Plant %d: %d mg fert, %d mL water\n",
            child_idx,fertilizer_mg,water_ml);
}

void print_initial(int idx) {
    dprintf(out_fd,"   \\|/\n  --%d--\n-------------------------\n",idx);
}

void print_growth(int idx,int rows) {
    dprintf(out_fd,"\nDay %d  \\|/\n         --%d--\n",rows,idx);
    for(int i=0;i<rows;i++) dprintf(out_fd,"          | |\n");
    dprintf(out_fd,"-------------------------\n");
}
