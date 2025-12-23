#include "../inc/execution.h"
#include "../inc/jobs.h"
#include <signal.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

extern Job *first_job;
extern int shell_terminal;
extern int shell_is_interactive;
extern pid_t shell_pgid;

int g_last_status = 0;
pid_t g_last_bg_pgid = 0;

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
        printf("[%d] %d %s %s\n", job->id, job->pgid, job_status_str(job->status), job->command);
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

int builtin_kill(char **argv) {
    // kill [-SIGNAL] <pid|%jobid>
    int sig = SIGTERM;
    int idx = 1;

    if (!argv[1]) {
        fprintf(stderr, "kill: usage: kill [-SIGNAL] <pid|%%jobid>\n");
        return 1;
    }

    if (argv[1][0] == '-' && argv[1][1] != '\0') {
        sig = atoi(argv[1] + 1);
        if (sig <= 0) {
            fprintf(stderr, "kill: bad signal: %s\n", argv[1]);
            return 1;
        }
        idx = 2;
    }

    if (!argv[idx]) {
        fprintf(stderr, "kill: usage: kill [-SIGNAL] <pid|%%jobid>\n");
        return 1;
    }

    if (argv[idx][0] == '%') {
        int job_id = atoi(argv[idx] + 1);
        Job *j = find_job_by_id(job_id);
        if (!j) {
            fprintf(stderr, "kill: job not found: %s\n", argv[idx]);
            return 1;
        }
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
    if (strcmp(argv[0], "cd") == 0) {
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

    if (strcmp(argv[0], "exit") == 0) {
        int code = argv[1] ? atoi(argv[1]) : 0;
        exit(code);
    }

    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
        else perror("getcwd");
        return 0;
    }

    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; argv[i]; ++i) {
            printf("%s%s", argv[i], argv[i + 1] ? " " : "");
        }
        printf("\n");
        return 0;
    }

    if (strcmp(argv[0], "help") == 0) {
        printf("MyBash builtins: cd, exit [n], help, pwd, echo, jobs, fg, bg, kill, set, unset\n");
        return 0;
    }

    if (strcmp(argv[0], "jobs") == 0) { print_jobs_list(); return 0; }
    if (strcmp(argv[0], "fg") == 0) return builtin_fg(argv);
    if (strcmp(argv[0], "bg") == 0) return builtin_bg(argv);
    if (strcmp(argv[0], "kill") == 0) return builtin_kill(argv);
    if (strcmp(argv[0], "set") == 0) return builtin_set(argv);
    if (strcmp(argv[0], "unset") == 0) return builtin_unset(argv);

    return 1;
}

int handle_redirection(Redirection *redir) {
    for (Redirection *r = redir; r; r = r->next) {
        int fd = -1;

        switch (r->type) {
            case REDIR_IN:
                fd = open(r->filename, O_RDONLY);
                if (fd < 0) { perror(r->filename); return 1; }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2"); close(fd); return 1; }
                close(fd);
                break;

            case REDIR_OUT:
                fd = open(r->filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) { perror(r->filename); return 1; }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return 1; }
                close(fd);
                break;

            case REDIR_APPEND:
                fd = open(r->filename, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd < 0) { perror(r->filename); return 1; }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return 1; }
                close(fd);
                break;

            case REDIR_ERR_OUT:
                fd = open(r->filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) { perror(r->filename); return 1; }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return 1; }
                if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2"); close(fd); return 1; }
                close(fd);
                break;

            case REDIR_ERR_APPEND:
                fd = open(r->filename, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd < 0) { perror(r->filename); return 1; }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return 1; }
                if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2"); close(fd); return 1; }
                close(fd);
                break;

            default:
                fprintf(stderr, "redir: unknown type\n");
                return 1;
        }
    }
    return 0;
}

int run_builtin_with_redir(ASTNode *node) {
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    if (saved_in < 0 || saved_out < 0 || saved_err < 0) {
        perror("dup");
        return 1;
    }

    if (handle_redirection(node->command.redir) != 0) {
        dup2(saved_in, STDIN_FILENO);
        dup2(saved_out, STDOUT_FILENO);
        dup2(saved_err, STDERR_FILENO);
        close(saved_in); close(saved_out); close(saved_err);
        return 1;
    }

    int rc = run_builtin(node->command.argv);

    dup2(saved_in, STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);
    close(saved_in); close(saved_out); close(saved_err);
    return rc;
}

void expand_argv(char **argv) {
    for (int i = 0; argv && argv[i]; i++) {
        if (argv[i][0] != '$') continue;

        char buf[32];
        const char *rep = "";

        if (strcmp(argv[i], "$?") == 0) {
            snprintf(buf, sizeof(buf), "%d", g_last_status);
            rep = buf;
        } else if (strcmp(argv[i], "$$") == 0) {
            snprintf(buf, sizeof(buf), "%d", (int)getpid());
            rep = buf;
        } else if (strcmp(argv[i], "$!") == 0) {
            snprintf(buf, sizeof(buf), "%d", (int)g_last_bg_pgid);
            rep = buf;
        } else {
            const char *env = getenv(argv[i] + 1);
            rep = env ? env : "";
        }

        char *dup = strdup(rep);
        if (!dup) {
            perror("strdup");
            continue;
        }

        free(argv[i]);
        argv[i] = dup;
    }
}




int exec_command_in_child(ASTNode *node) {
    char **argv = node->command.argv;
    if (!argv || !argv[0]) _exit(0);

    expand_argv(argv);

    if (handle_redirection(node->command.redir) != 0) _exit(1);

    if (is_builtin(argv[0])) {
        int rc = run_builtin(argv);
        _exit(rc);
    }

    execvp(argv[0], argv);
    fprintf(stderr, "%s: command not found\n", argv[0]);
    _exit(127);
}

int flatten_pipeline(ASTNode *node, ASTNode ***stages_out, int **pipe_stderr_out, int *n_out) {
    int cap = 8, nst_rev = 0, nop_rev = 0;
    ASTNode **st_rev = malloc(sizeof(ASTNode*) * cap);
    int *ops_rev = malloc(sizeof(int) * cap);
    if (!st_rev || !ops_rev) { free(st_rev); free(ops_rev); return 1; }

    ASTNode *cur = node;
    while (cur && (cur->type == NODE_PIPE || cur->type == NODE_PIPE_STDERR)) {
        if (nst_rev + 1 >= cap) {
            cap *= 2;

            ASTNode **ns = realloc(st_rev, sizeof(ASTNode*) * cap);
            if (!ns) { free(st_rev); free(ops_rev); return 1; }
            st_rev = ns;

            int *no = realloc(ops_rev, sizeof(int) * cap);
            if (!no) { free(st_rev); free(ops_rev); return 1; }
            ops_rev = no;
        }

        st_rev[nst_rev++] = cur->binary.right;
        ops_rev[nop_rev++] = (cur->type == NODE_PIPE_STDERR) ? 1 : 0;
        cur = cur->binary.left;
    }

    if (nst_rev + 1 >= cap) {
        cap += 1;
        ASTNode **ns = realloc(st_rev, sizeof(ASTNode*) * cap);
        if (!ns) { free(st_rev); free(ops_rev); return 1; }
        st_rev = ns;
    }
    st_rev[nst_rev++] = cur; // leftmost

    int nst = nst_rev;
    ASTNode **st = malloc(sizeof(ASTNode*) * nst);
    int *pipe_stderr = calloc((nst > 1) ? (nst - 1) : 1, sizeof(int));
    if (!st || !pipe_stderr) {
        free(st_rev); free(ops_rev); free(st); free(pipe_stderr);
        return 1;
    }

    for (int i = 0; i < nst; i++) {
        st[i] = st_rev[nst - 1 - i];
    }

    for (int k = 0; k < nop_rev; k++) {
        int edge_idx = (nst - 2) - k;
        if (edge_idx >= 0 && edge_idx < nst - 1) {
            pipe_stderr[edge_idx] = ops_rev[k];
        }
    }

    free(st_rev);
    free(ops_rev);

    *stages_out = st;
    *pipe_stderr_out = pipe_stderr;
    *n_out = nst;
    return 0;
}




int wait_foreground_pgid(pid_t pgid, pid_t last_pid) {
    int last_status = 0;

    while (1) {
        int status = 0;
        pid_t pid = waitpid(-pgid, &status, WUNTRACED);
        if (pid < 0) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) break;
            perror("waitpid");
            break;
        }

        if (pid == last_pid) {
            last_status = status;
        }

        if (WIFSTOPPED(status)) {
            Job *j = find_job_by_pgid(pgid);
            if (j) j->status = JOB_STOPPED;
            break;
        }
    }

    if (WIFEXITED(last_status)) return WEXITSTATUS(last_status);
    if (WIFSIGNALED(last_status)) return 128 + WTERMSIG(last_status);
    return 0;
}

int execute_pipeline_node(ASTNode *node) {
    ASTNode **stages = NULL;
    int *pipe_stderr = NULL;
    int nst = 0;

    if (flatten_pipeline(node, &stages, &pipe_stderr, &nst) != 0) {
        fprintf(stderr, "pipeline: flatten failed\n");
        return 1;
    }

    if (nst <= 0) { free(stages); free(pipe_stderr); return 1; }
    if (nst == 1) {
        int rc = execute_internal(stages[0], 0);
        free(stages);
        free(pipe_stderr);
        return rc;
    }

    int pipes_count = nst - 1;
    int (*pipes)[2] = calloc(pipes_count, sizeof(int[2]));
    pid_t *pids = calloc(nst, sizeof(pid_t));
    if (!pipes || !pids) {
        perror("calloc");
        free(pipes); free(pids);
        free(stages); free(pipe_stderr);
        return 1;
    }

    for (int i = 0; i < pipes_count; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int k = 0; k < i; k++) { close(pipes[k][0]); close(pipes[k][1]); }
            free(pipes); free(pids);
            free(stages); free(pipe_stderr);
            return 1;
        }
    }

    pid_t pgid = 0;

    for (int i = 0; i < nst; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // best-effort cleanup
            for (int k = 0; k < pipes_count; k++) { close(pipes[k][0]); close(pipes[k][1]); }
            free(pipes); free(pids);
            free(stages); free(pipe_stderr);
            return 1;
        }

        if (pid == 0) {
            // child: restore default signals
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            // process group
            if (i == 0) setpgid(0, 0);
            else setpgid(0, pgid);

            // connect stdin
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            // connect stdout (+ optionally stderr)
            if (i < nst - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
                if (pipe_stderr[i]) {
                    dup2(pipes[i][1], STDERR_FILENO);
                }
            }

            for (int k = 0; k < pipes_count; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }

            if (stages[i] && stages[i]->type == NODE_COMMAND) {
                exec_command_in_child(stages[i]);
            } else {
                int rc = execute_internal(stages[i], 1);
                _exit(rc);
            }
        }

        // parent: set group deterministically
        if (i == 0) {
            pgid = pid;
            setpgid(pid, pgid);
        } else {
            setpgid(pid, pgid);
        }

        pids[i] = pid;
    }

    for (int k = 0; k < pipes_count; k++) {
        close(pipes[k][0]);
        close(pipes[k][1]);
    }

    int rc = 0;
    if (shell_is_interactive) {
        add_job(pgid, "pipeline", JOB_RUNNING, 0);

        tcsetpgrp(shell_terminal, pgid);
        rc = wait_foreground_pgid(pgid, pids[nst - 1]);
        tcsetpgrp(shell_terminal, shell_pgid);

        Job *j = find_job_by_pgid(pgid);
        if (j && j->status != JOB_STOPPED) {
            j->status = JOB_DONE;
            delete_job(pgid);
        }
    } else {
        int status = 0;
        for (int i = 0; i < nst; i++) {
            waitpid(pids[i], &status, 0);
            if (i == nst - 1) {
                if (WIFEXITED(status)) rc = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) rc = 128 + WTERMSIG(status);
                else rc = 1;
            }
        }
    }

    free(pipes);
    free(pids);
    free(stages);
    free(pipe_stderr);
    return rc;
}

int execute_internal(ASTNode *node, int in_child) {
    if (!node) return 1;

    switch (node->type) {
        case NODE_COMMAND:
            if (in_child) {
                exec_command_in_child(node);
                return 1;
            }
            return execute_command(node);

        case NODE_PIPE:
        case NODE_PIPE_STDERR:
            return execute_pipeline_node(node);

        case NODE_SEQUENCE:
            (void)execute_internal(node->binary.left, in_child);
            return execute_internal(node->binary.right, in_child);

        case NODE_AND: {
            int l = execute_internal(node->binary.left, in_child);
            if (l == 0) return execute_internal(node->binary.right, in_child);
            return l;
        }

        case NODE_OR: {
            int l = execute_internal(node->binary.left, in_child);
            if (l != 0) return execute_internal(node->binary.right, in_child);
            return 0;
        }

        case NODE_BACKGROUND: {
            pid_t pid = fork();
            if (pid == 0) {
                setpgid(0, 0);
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                _exit(execute_internal(node->unary.child, 1));
            } else if (pid > 0) {
                setpgid(pid, pid);

                const char *cmd_name = "background";
                if (node->unary.child &&
                    node->unary.child->type == NODE_COMMAND &&
                    node->unary.child->command.argv &&
                    node->unary.child->command.argv[0]) {
                    cmd_name = node->unary.child->command.argv[0];
                }

                add_job(pid, cmd_name, JOB_RUNNING, 1);
                g_last_bg_pgid = pid;

                Job *j = find_job_by_pgid(pid);
                if (j) printf("[%d] %d\n", j->id, pid);

                return 0;
            } else {
                perror("fork background");
                return 1;
            }
        }

        case NODE_SUB: {
            pid_t pid = fork();
            if (pid == 0) {
                _exit(execute_internal(node->unary.child, 1));
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) return WEXITSTATUS(status);
                if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
                return 1;
            } else {
                perror("fork subshell");
                return 1;
            }
        }

        case NODE_GROUP:
            return execute_internal(node->unary.child, in_child);

        default:
            fprintf(stderr, "execute: unknown node type\n");
            return 1;
    }
}

int execute(ASTNode *node) {
    int rc = execute_internal(node, 0);
    g_last_status = rc;
    return rc;
}

int execute_command(ASTNode *node) {
    char **argv = node->command.argv;
    if (!argv || !argv[0]) return 0;

    expand_argv(argv);

    if (is_builtin(argv[0])) {
        return run_builtin_with_redir(node);
    }

    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);

        if (shell_is_interactive) {
            tcsetpgrp(shell_terminal, getpid());
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
        }

        if (handle_redirection(node->command.redir) != 0) _exit(1);

        execvp(argv[0], argv);
        fprintf(stderr, "%s: command not found\n", argv[0]);
        _exit(127);

    } else if (pid > 0) {
        setpgid(pid, pid);

        if (shell_is_interactive) {
            add_job(pid, argv[0], JOB_RUNNING, 0);

            tcsetpgrp(shell_terminal, pid);
            int rc = wait_foreground_pgid(pid, pid);
            tcsetpgrp(shell_terminal, shell_pgid);

            Job *j = find_job_by_pgid(pid);
            if (j && j->status != JOB_STOPPED) {
                j->status = JOB_DONE;
                delete_job(pid);
            }

            return rc;
        } else {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) return WEXITSTATUS(status);
            if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
            return 1;
        }
    } else {
        perror("fork failed");
        return 1;
    }
}
