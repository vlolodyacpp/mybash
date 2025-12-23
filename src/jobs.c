#include "../inc/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

Job *first_job = NULL;
pid_t shell_pgid;
int shell_terminal;
int shell_is_interactive;


void init_shell() {
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);

    if(shell_is_interactive) { 
        while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())){
            kill(-shell_pgid, SIGTTIN);
        }

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);


        shell_pgid = getpid();
        if(setpgid(shell_pgid, shell_pgid) < 0){
            perror("error setpgid");
            exit(1);
        }

        tcsetpgrp(shell_terminal, shell_pgid);
    }
}

void add_job(pid_t pgid, const char *command, JobStatus status, int is_bg) {

    Job *jobs_list = malloc(sizeof(Job));
    if(!jobs_list){
        perror("malloc");
        return;
    }

    jobs_list -> pgid = pgid;
    jobs_list -> command = strdup(command);
    jobs_list -> status = status;
    jobs_list -> is_background = is_bg;
    jobs_list -> next = NULL;

    int max_id = 0;
    Job *curr = first_job;
    while (curr) {
        if(curr -> id > max_id) max_id = curr -> id;
        curr = curr -> next;
    }
    jobs_list -> id = max_id + 1;

    if (!first_job) { 
        first_job = jobs_list;
    } else { 
        curr = first_job;
        while (curr -> next) curr = curr -> next;
        curr -> next = jobs_list;
    } 

}

void delete_job(pid_t pgid) { 
    Job *curr = first_job;
    Job *prev = NULL;

    while (curr) {
        if (curr -> pgid == pgid) { 
            if (prev) {
                prev -> next = curr -> next;
            } else {
                first_job = curr -> next;
            }
            free(curr -> command);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr -> next;
    }
}

Job *find_job_by_pgid(pid_t pgid) { 
    Job *jobs_list = first_job;
    while (jobs_list) { 
        if (jobs_list -> pgid == pgid) return jobs_list;
        jobs_list = jobs_list -> next;
    }
    return NULL;
}

Job *find_job_by_id(int id) { 
    Job *jobs_list = first_job;
    while (jobs_list) { 
        if (jobs_list -> id == id) return jobs_list;
        jobs_list = jobs_list -> next;
    }
    return NULL;
}

void wait_for_job(Job *jobs_list) { 
    int status;
    pid_t pid;

    while ((pid = waitpid(-jobs_list -> pgid, &status, WUNTRACED)) > 0) { 
        if(WIFSTOPPED(status)) { 
            jobs_list -> status = JOB_STOPPED;
            jobs_list -> is_background = 1;
            printf("\n[%d]+ Stopped %s\n", jobs_list -> id, jobs_list -> command);
            return;
        }
        if(WIFEXITED(status)) { 
            jobs_list -> status = JOB_DONE;
        }
    }

    if (pid == -1 && errno == ECHILD) { 
        jobs_list -> status = JOB_DONE;
    }

    if (jobs_list -> status == JOB_DONE && jobs_list -> id > 0) {
        delete_job(jobs_list -> pgid);
    }
}

void check_background_jobs() {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        Job *job = find_job_by_pgid(pid);
        if (job) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                printf("[%d] Done %s\n", job->id, job->command);
                delete_job(pid);
            }
            else if (WIFSTOPPED(status)) {
                job->status = JOB_STOPPED;
                printf("[%d]+ Stopped %s\n", job->id, job->command);
            }
        }
    }
}

int builtin_fg(char **args) {
    if (!args[1]) {
        fprintf(stderr, "fg: usage: fg <job_id>\n");
        return 1;
    }
    
    int job_id = atoi(args[1]); 
    Job *jobs_list = find_job_by_id(job_id);
    if (!jobs_list) {
        fprintf(stderr, "fg: job not found: %s\n", args[1]);
        return 1;
    }

    jobs_list -> is_background = 0;

    tcsetpgrp(shell_terminal, jobs_list->pgid);


    if (jobs_list->status == JOB_STOPPED) {
        kill(-jobs_list->pgid, SIGCONT);
        jobs_list->status = JOB_RUNNING;
    }


    wait_for_job(jobs_list);

   
    tcsetpgrp(shell_terminal, shell_pgid);
    return 0;
}

int builtin_bg(char **args) {
    if (!args[1]) {
        fprintf(stderr, "bg: usage: bg <job_id>\n");
        return 1;
    }

    int job_id = atoi(args[1]);
    Job *jobs_list = find_job_by_id(job_id);
    if (!jobs_list) {
        fprintf(stderr, "bg: job not found\n");
        return 1;
    }

    if (jobs_list -> status == JOB_STOPPED) {
        kill(-jobs_list->pgid, SIGCONT);
        jobs_list -> status = JOB_RUNNING;
        printf("[%d]+ %s &\n", jobs_list -> id, jobs_list -> command);
    }
    return 0;
}