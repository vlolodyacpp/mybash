#pragma once 

#include <sys/types.h>

typedef enum { 
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobStatus;

typedef struct Job{
    char *command;
    pid_t pgid;
    int id;
    JobStatus status;
    int is_background;
    struct Job *next;
} Job;

extern pid_t shell_pgid;
extern int shell_terminal;
extern int shell_is_interactive;

void init_shell();
void add_job(pid_t, const char*, JobStatus, int);
void delete_job(pid_t);
Job *find_job_by_pgid(pid_t);
Job *find_job_by_id(int);
void wait_for_job(Job *);
void check_background_jobs();

int builtin_jobs(char **);
int builtin_fg(char **);
int builtin_bg(char **);





