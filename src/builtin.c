#include "../inc/builtin.h"
#include "../inc/execution.h"
#include "../inc/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

extern Job *first_job;
extern pid_t shell_pgid;              
extern int shell_terminal;


const char *job_status_str(JobStatus st) {
    switch (st) {
        case JOB_RUNNING: return "Running";
        case JOB_STOPPED: return "Stopped";
        case JOB_DONE:    return "Done";
        default:          return "Unknown";
    }
}

void print_jobs_list() {
    Job *job = first_job;
    while (job) {
        printf("[%d] %d %s %s\n", job->id, job->pgid, 
               job_status_str(job->status), job->command);
        job = job->next;
    }
}

int is_builtin(const char *s) {
    if (!s) return 0;
    return strcmp(s, "cd") == 0 ||
           strcmp(s, "exit") == 0 ||
           strcmp(s, "pwd") == 0 ||
           strcmp(s, "echo") == 0 ||
           strcmp(s, "help") == 0 ||
           strcmp(s, "jobs") == 0 ||
           strcmp(s, "fg") == 0 ||
           strcmp(s, "bg") == 0 ||
           strcmp(s, "kill") == 0 ||
           strcmp(s, "set") == 0 ||
           strcmp(s, "unset") == 0;
}     

int builtin_cd(char **argv) {
    // argv[0] = "cd", argv[1] = путь (или NULL)
    const char *path = argv[1] ? argv[1] : getenv("HOME");
    if (!path) {
        fprintf(stderr, "cd: HOME not set\n");
        return 1;
    }
    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int builtin_exit(char **argv) {
    // argv[1] может содержать код выхода
    int code = argv[1] ? atoi(argv[1]) : 0;
    exit(code);
}

int builtin_echo(char **argv) {
    for (int i = 1; argv[i]; ++i) {
        printf("%s%s", argv[i], argv[i + 1] ? " " : "");
    }
    printf("\n");
    return 0;
}

int builtin_pwd(char **argv) {
    (void)argv;
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd");
        return 1;
    }
    return 0;
}

int builtin_help(char **argv) {
    (void)argv;
    printf("MyBash builtins:\n");
    printf("  cd [dir]          - Change directory\n");
    printf("  pwd               - Print working directory\n");
    printf("  echo [args...]    - Print arguments\n");
    printf("  exit [n]          - Exit shell with code n\n");
    printf("  help              - Show this help\n");
    printf("  jobs              - List background jobs\n");
    printf("  fg %%jobid         - Move job to foreground\n");
    printf("  bg %%jobid         - Continue job in background\n");
    printf("  kill [-SIG] <pid> - Send signal to process\n");
    printf("  set VAR=value     - Set environment variable\n");
    printf("  unset VAR         - Unset environment variable\n");
    return 0;
}

int builtin_jobs(char **argv) {
    (void)argv;
    print_jobs_list();
    return 0;
}

int builtin_kill(char **argv) {
    // kill [-SIGNAL] <pid|%jobid>
    int sig = SIGTERM;
    int idx = 1;


    // Проверка на наличие аргументов
    if (!argv[1]) {
        fprintf(stderr, "kill: usage: kill [-SIGNAL] <pid|%%jobid>\n");
        return 1;
    }

    // Парсинг номера сигнала
    if (argv[1][0] == '-' && argv[1][1] != '\0') {
        sig = atoi(argv[1] + 1);
        if (sig <= 0) {
            fprintf(stderr, "kill: bad signal: %s\n", argv[1]);
            return 1;
        }
        idx = 2;
    }

    // Проверка на наличие PID
    if (!argv[idx]) {
        fprintf(stderr, "kill: usage: kill [-SIGNAL] <pid|%%jobid>\n");
        return 1;
    }

    // Обработка jobid
    if (argv[idx][0] == '%') {
        //  Поиск задачи в списке задач
        int job_id = atoi(argv[idx] + 1);
        Job *j = find_job_by_id(job_id);
        
        if (!j) {
            fprintf(stderr, "kill: job not found: %s\n", argv[idx]);
            return 1;
        }

        //когда указываем отрицательный pgid, то убиваем весь конвеер(при его наличии)
        if (kill(-j->pgid, sig) != 0) {
            perror("kill");
            return 1;
        }
        return 0;
    }

    pid_t pid = (pid_t)atoi(argv[idx]);
    if (pid <= 0) {
        fprintf(stderr, "kill: bad pid: %s\n", argv[idx]);
        return 1;
    }
    if (kill(pid, sig) != 0) {
        perror("kill");
        return 1;
    }
    return 0;
}

int builtin_set(char **argv) {
    // set VAR=value
    if (!argv[1]) {
        fprintf(stderr, "set: usage: set VAR=value\n");
        return 1;
    }

    char *eq = strchr(argv[1], '=');

    if (!eq || eq == argv[1]) {
        fprintf(stderr, "set: usage: set VAR=value\n");
        return 1;
    }

    //разделяем строку для взятия аргумента и значения
    *eq = '\0';
    const char *name = argv[1];
    const char *val = eq + 1;

    int rc = setenv(name, val, 1);
    *eq = '=';

    if (rc != 0) {
        perror("setenv");
        return 1;
    }
    return 0;
}

int builtin_unset(char **argv) {
    if (!argv[1]) {
        fprintf(stderr, "unset: usage: unset VAR\n");
        return 1;
    }
    if (unsetenv(argv[1]) != 0) {
        perror("unsetenv");
        return 1;
    }
    return 0;
}

int run_builtin(char **argv) {
    if (strcmp(argv[0], "cd") == 0)     return builtin_cd(argv);
    if (strcmp(argv[0], "exit") == 0)   return builtin_exit(argv);
    if (strcmp(argv[0], "pwd") == 0)    return builtin_pwd(argv);
    if (strcmp(argv[0], "echo") == 0)   return builtin_echo(argv);
    if (strcmp(argv[0], "help") == 0)   return builtin_help(argv);
    if (strcmp(argv[0], "jobs") == 0)   return builtin_jobs(argv);
    if (strcmp(argv[0], "fg") == 0)     return builtin_fg(argv);
    if (strcmp(argv[0], "bg") == 0)     return builtin_bg(argv);
    if (strcmp(argv[0], "kill") == 0)   return builtin_kill(argv);
    if (strcmp(argv[0], "set") == 0)    return builtin_set(argv);
    if (strcmp(argv[0], "unset") == 0)  return builtin_unset(argv);
    
    return 1;  // Неизвестная команда
}

int run_builtin_with_redir(ASTNode *node) {
    // Сохраняем оригинальные дескрипторы
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    
    if (saved_in < 0 || saved_out < 0 || saved_err < 0) {
        perror("dup");
        return 1;
    }

    // Применяем перенаправления
    if (handle_redirection(node->command.redir) != 0) {
        dup2(saved_in, STDIN_FILENO);
        dup2(saved_out, STDOUT_FILENO);
        dup2(saved_err, STDERR_FILENO);
        close(saved_in);
        close(saved_out);
        close(saved_err);
        return 1;
    }

    // Выполняем встроенную команду
    int rc = run_builtin(node->command.argv);

    // Восстанавливаем оригинальные дескрипторы
    dup2(saved_in, STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);
    close(saved_in);
    close(saved_out);
    close(saved_err);
    
    return rc;
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
