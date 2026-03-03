#include "../inc/execution.h"
#include "../inc/jobs.h"
#include "../inc/builtin.h"
#include <signal.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

extern Job *first_job;             // голова списка фоновых заданий
extern int shell_terminal;         // файловый дескриптор терминала (обычно STDIN)
extern int shell_is_interactive;   // работаем ли в интерактивном режиме
extern pid_t shell_pgid;           // идентификатор группы процессов shell

// код завершения последней выполненной команды (для $?)
int g_last_status = 0;

// пид последнего фонового задания (для $!)
pid_t g_last_bg_pgid = 0;

typedef struct {
    ASTNode **stages;    
    int     *pipe_stderr; 
    int      count;       
} Pipeline;

static int count_pipe_depth(ASTNode *node) {
    int n = 1;
    while (node && (node->type == NODE_PIPE || node->type == NODE_PIPE_STDERR)) {
        n++;
        node = node->binary.left;
    }
    return n;
}

int handle_redirection(Redirection *redir) {
    for (Redirection *r = redir; r; r = r->next) {
        int fd = -1;  

        switch (r->type) {
            case REDIR_IN:  // < 
                fd = open(r->filename, O_RDONLY);
                if (fd < 0) { 
                    perror(r->filename); 
                    return 1; 
                }  
                // копируем дескриптор fd 
                if (dup2(fd, STDIN_FILENO) < 0) { 
                    perror("dup2"); 
                    close(fd); 
                    return 1; 
                }

                close(fd);  
                break;

            case REDIR_OUT:  
                fd = open(r->filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) { 
                    perror(r->filename); 
                    return 1; 
                } 

                if (dup2(fd, STDOUT_FILENO) < 0) { 
                    perror("dup2"); close(fd); 
                    return 1; 
                }

                close(fd);  
                break;

            case REDIR_APPEND: 
                fd = open(r->filename, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd < 0) { 
                    perror(r->filename); 
                    return 1; 
                }  
                
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2"); 
                    close(fd); 
                    return 1;
                }
                close(fd); 
                break;

            case REDIR_ERR_OUT: 
                fd = open(r->filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) { 
                    perror(r->filename); 
                    return 1; 
                }  
               
                if (dup2(fd, STDOUT_FILENO) < 0) { 
                    perror("dup2"); \
                    close(fd); 
                    return 1; 
                }
    
                if (dup2(fd, STDERR_FILENO) < 0) { 
                    perror("dup2"); 
                    close(fd); 
                    return 1; 
                }
                close(fd);  
                break;

            case REDIR_ERR_APPEND:  
                fd = open(r->filename, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd < 0) { 
                    perror(r->filename); 
                    return 1; 
                }
                if (dup2(fd, STDOUT_FILENO) < 0) { 
                    perror("dup2"); 
                    close(fd); 
                    return 1; 
                }
                if (dup2(fd, STDERR_FILENO) < 0) { 
                    perror("dup2"); 
                    close(fd); 
                    return 1; 
                }
                close(fd);  
                break;

            default:
                fprintf(stderr, "redir: unknown type\n");  
                return 1; 
        }
    }
    return 0;  
}

int append_slice(char **dst, size_t *len, size_t *cap, const char *src, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t new_cap = *cap;
        while (*len + n + 1 > new_cap) new_cap *= 2;
        char *new_dst = realloc(*dst, new_cap);
        if (!new_dst) return 0;
        *dst = new_dst;
        *cap = new_cap;
    }
    memcpy(*dst + *len, src, n);
    *len += n;
    (*dst)[*len] = '\0';
    return 1;
}

char *expand_one_arg(const char *arg) {
    size_t cap = 64;
    size_t out_len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    for (size_t i = 0; arg[i] != '\0'; ) {
        if (arg[i] != '$') {
            if (!append_slice(&out, &out_len, &cap, &arg[i], 1)) {
                free(out);
                return NULL;
            }
            i++;
            continue;
        }

        char buf[64];
        const char *rep = "";
        size_t consumed = 1;

        if (arg[i + 1] == '?') {
            snprintf(buf, sizeof(buf), "%d", g_last_status);
            rep = buf;
            consumed = 2;
        } else if (arg[i + 1] == '$') {
            snprintf(buf, sizeof(buf), "%d", (int)getpid());
            rep = buf;
            consumed = 2;
        } else if (arg[i + 1] == '!') {
            snprintf(buf, sizeof(buf), "%d", (int)g_last_bg_pgid);
            rep = buf;
            consumed = 2;
        } else if (isalpha((unsigned char)arg[i + 1]) || arg[i + 1] == '_') {
            size_t start = i + 1;
            size_t end = start;
            while (isalnum((unsigned char)arg[end]) || arg[end] == '_') end++;

            size_t name_len = end - start;
            char *name = malloc(name_len + 1);
            if (!name) {
                free(out);
                return NULL;
            }
            memcpy(name, arg + start, name_len);
            name[name_len] = '\0';

            const char *env = getenv(name);
            rep = env ? env : "";
            free(name);
            consumed = end - i;
        } else {
            if (!append_slice(&out, &out_len, &cap, "$", 1)) {
                free(out);
                return NULL;
            }
            i++;
            continue;
        }

        if (!append_slice(&out, &out_len, &cap, rep, strlen(rep))) {
            free(out);
            return NULL;
        }
        i += consumed;
    }

    return out;
}

void expand_argv(char **argv, int *arg_no_expand) {
    for (int i = 0; argv && argv[i]; i++) {
        if (arg_no_expand && arg_no_expand[i]) continue;

        char *expanded = expand_one_arg(argv[i]);
        if (!expanded) {
            perror("expand");
            continue;
        }
        free(argv[i]);
        argv[i] = expanded;
    }
}


int exec_command_in_child(ASTNode *node) {
    char **argv = node->command.argv;

    if (!argv || !argv[0]) _exit(0); 
    
    expand_argv(argv, node->command.arg_no_expand);

    // выполняем все перенаправления
    if (handle_redirection(node->command.redir) != 0) 
        _exit(1); 
    // встроенные команды
    if (is_builtin(argv[0])) {
        int rc = run_builtin(argv);
        _exit(rc);  
    }

    execvp(argv[0], argv);
    
    // оказались тут - execvp не отработал
    fprintf(stderr, "%s: command not found\n", argv[0]);  
    _exit(127);  
}

// разворачиваем дерево пайпов в плоский массив команд
Pipeline flatten_pipeline(ASTNode *node) {
    Pipeline p = {NULL, NULL, 0};

    int count = count_pipe_depth(node);

    p.stages = malloc(sizeof(ASTNode *) * count);
    p.pipe_stderr = calloc((count > 1) ? (count - 1) : 1, sizeof(int));

    if (!p.stages || !p.pipe_stderr) {
        free(p.stages);
        free(p.pipe_stderr);
        p.stages = NULL;
        return p;
    }

    // спускаемся по левой ветке, правые дети — в массив
    int idx = count - 1;
    ASTNode *cur = node;

    while (cur && (cur->type == NODE_PIPE || cur->type == NODE_PIPE_STDERR)) {
        p.stages[idx] = cur->binary.right;
        if (idx > 0)
            p.pipe_stderr[idx - 1] = (cur->type == NODE_PIPE_STDERR) ? 1 : 0;
        idx--;
        cur = cur->binary.left;
    }
    p.stages[0] = cur; // самая левая команда

    p.count = count;
    return p;
}


int wait_foreground_pgid(pid_t pgid, pid_t last_pid) {
    int last_status = 0;  // завершения последнего процесса 

    // ожидание процессов в группе
    while (1) {
        int status = 0;  // завершения процесса
        

        // Ждем завершение всей группы, возвращаем управление, если приостановили 
        pid_t pid = waitpid(-pgid, &status, WUNTRACED);
        
        if (pid < 0) {  // Ошибка waitpid
            if (errno == EINTR) continue;  // прервано сигналом, продолжаем ждать
            if (errno == ECHILD) break;    // нет больше дочерних процессов, выходим из цикла
            perror("waitpid");  
            break;  
        }

        // последний процесс в конвейере, сохраняем его статус
        if (pid == last_pid) {
            last_status = status;  
        }

        // приостановили работу, ищем какой и обновляем его статус
        if (WIFSTOPPED(status)) {
            Job *j = find_job_by_pgid(pgid);
            if (j) j -> status = JOB_STOPPED; 
            break;  
        }
    }

    // закончилось само - код возврата
    if (WIFEXITED(last_status)) 
        return WEXITSTATUS(last_status);  // WEXITSTATUS: извлекаем код возврата
    
    //роцесс завершен сигналом 
    if (WIFSIGNALED(last_status)) 
        return 128 + WTERMSIG(last_status);  // 128 + номер сигнала - код сигнала
    
    return 0;  
}

int execute_pipeline_node(ASTNode *node) {
    Pipeline p = flatten_pipeline(node);

    if (!p.stages || p.count <= 0) {
        fprintf(stderr, "pipeline: flatten failed\n");
        free(p.stages);
        free(p.pipe_stderr);
        return 1;
    }

    // команда без пайпа
    if (p.count == 1) { 
        int rc = execute_internal(p.stages[0], 0);
        free(p.stages);
        free(p.pipe_stderr);
        return rc;
    }

    int pipes_count = p.count - 1;
    int (*pipes)[2] = calloc(pipes_count, sizeof(int[2])); //массив пар для пайпа(r/w)
    pid_t *pids = calloc(p.count, sizeof(pid_t)); // массив пидов дочерок
    if (!pipes || !pids) {
        perror("calloc");
        free(pipes); 
        free(pids);
        free(p.stages); 
        free(p.pipe_stderr);
        return 1;
    }

    for (int i = 0; i < pipes_count; i++) {
        // создаем пайпы и чистим если не создались
        if (pipe(pipes[i]) < 0) {
            perror("pipe");

            for (int k = 0; k < i; k++) { 
                close(pipes[k][0]); 
                close(pipes[k][1]); 
            }

            free(pipes); 
            free(pids);
            free(p.stages); 
            free(p.pipe_stderr);
            return 1;
        }
    }

    pid_t pgid = 0;

    for (int i = 0; i < p.count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // закрываемся и освобождаем память
            for (int k = 0; k < pipes_count; k++) { 
                close(pipes[k][0]); 
                close(pipes[k][1]); 
            }
            free(pipes); 
            free(pids);
            free(p.stages); 
            free(p.pipe_stderr);
            return 1;
        }

        if (pid == 0) {
            // Стандартная обработка сигналов
            signal(SIGINT, SIG_DFL);  //ctrl + c
            signal(SIGQUIT, SIG_DFL); //ctrl + '\'
            signal(SIGTSTP, SIG_DFL); //ctrl + Z
            signal(SIGTTIN, SIG_DFL); // read
            signal(SIGTTOU, SIG_DFL); // write

            // создаем группу процессов
            if (i == 0){
                setpgid(0, 0);
            } else {
                setpgid(0, pgid);
            }

            // перенаправление stdin не первая команда -> читаем из pipe
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            // перенаправление stdout не первая команда -> пишем в pipe
            if (i < p.count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
                if (p.pipe_stderr[i]) {
                    dup2(pipes[i][1], STDERR_FILENO);
                }
            }

            //закрываем после дупов
            for (int k = 0; k < pipes_count; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }

            if (p.stages[i] && p.stages[i]->type == NODE_COMMAND) {
                exec_command_in_child(p.stages[i]);
            } else {
                int rc = execute_internal(p.stages[i], 1);
                _exit(rc);
            }
        }

        // устанавливаем группу процессов тоже
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

    // Обработка конвеера
    if (shell_is_interactive) {

        // конвеер как новый job, пока выполняется управление у конвеера
        add_job(pgid, "pipeline", JOB_RUNNING, 0);

        tcsetpgrp(shell_terminal, pgid);
        rc = wait_foreground_pgid(pgid, pids[p.count - 1]);
        tcsetpgrp(shell_terminal, shell_pgid);

        // после завершения возвращаем управление шеллу и обновляем статус
        Job *j = find_job_by_pgid(pgid);
        if (j && j->status != JOB_STOPPED) {
            j->status = JOB_DONE;
            delete_job(pgid);
        }
    } else { // простое ожидание пока не закончится
        int status = 0;
        for (int i = 0; i < p.count; i++) {
            waitpid(pids[i], &status, 0);
            if (i == p.count - 1) {
                if (WIFEXITED(status)) rc = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) rc = 128 + WTERMSIG(status);
                else rc = 1;
            }
        }
    }

    free(pipes);
    free(pids);
    free(p.stages);
    free(p.pipe_stderr);
    return rc;
}

int execute(ASTNode *node) {
    int rc = execute_internal(node, 0);
    g_last_status = rc;
    return rc;
}

int execute_internal(ASTNode *node, int in_child) {
    if (!node) return 1;

    switch (node->type) {
        //выполнение команды
        case NODE_COMMAND:
            if (in_child) {
                exec_command_in_child(node);
                return 1;
            }
            return execute_command(node);

        // для двух пайпов перенаправляем
        case NODE_PIPE:
        case NODE_PIPE_STDERR:
            return execute_pipeline_node(node);

        // последовательное выполнение - код выходной левой команды не запоминаем
        case NODE_SEQUENCE:
            execute_internal(node->binary.left, in_child);
            return execute_internal(node->binary.right, in_child);

        // сначала левую, потом правую
        case NODE_AND: {
            int l = execute_internal(node->binary.left, in_child);
            if (l == 0) return execute_internal(node->binary.right, in_child);
            return l;
        }

        // или левую или правую
        case NODE_OR: {
            int l = execute_internal(node->binary.left, in_child);
            if (l != 0) return execute_internal(node->binary.right, in_child);
            return l;
        }
        // фонове задание
        case NODE_BACKGROUND: {
            //дочерка
            pid_t pid = fork(); //делаем лидером собственной группы процессов
            if (pid == 0) { 
                setpgid(0, 0);

                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL); // востанавливаем обработки сигналов
                signal(SIGTSTP, SIG_DFL);
                _exit(execute_internal(node->unary.child, 1)); // выполнили и ушли
            // родительский
            } else if (pid > 0) { 
                setpgid(pid, pid); // устанавливаем группу в родителе

                // имя для jobs
                const char *cmd_name = "background"; 
                if (node->unary.child && node -> unary.child -> type == NODE_COMMAND 
                                      && node -> unary.child -> command.argv &&
                                         node -> unary.child -> command.argv[0]) {

                    cmd_name = node->unary.child->command.argv[0];
                }

                add_job(pid, cmd_name, JOB_RUNNING, 1); // делаем новое фон задание
                g_last_bg_pgid = pid; // для переменной $!

                Job *j = find_job_by_pgid(pid);
                if (j) printf("[%d] %d\n", j->id, pid); //вывод найденной работы

                return 0;
            } else {
                perror("fork background");
                return 1;
            }
        }
        // ( (cmd) )
        case NODE_SUB: {
            pid_t pid = fork();
            // дочерка
            if (pid == 0) {
                _exit(execute_internal(node->unary.child, 1)); // выполняем и выходим
                // изменения внутри () не влияют на шелл
            } else if (pid > 0) {
                // у родителя ждем завершения и разбираем код возврата
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
            return execute_internal(node->unary.child, in_child); // команды в {} влияют на шелл, обрабатываем 

        default:
            fprintf(stderr, "execute: unknown node type\n");
            return 1;
    }
}


int execute_command(ASTNode *node) {
    char **argv = node->command.argv;

    if (!argv || !argv[0]) return 0; 

    // расширяем переменные окружения в аргументах 
    expand_argv(argv, node->command.arg_no_expand);

    // встроенная ли команда
    if (is_builtin(argv[0])) {
        return run_builtin_with_redir(node);
    }

    // создаем новый дочерний процесс для выполнения внешней команды
    pid_t pid = fork();
    
    if (pid == 0) {  
        // дочерний процесс лидером своей группы процессов
        setpgid(0, 0);

        if (shell_is_interactive) {  // Если работаем в интерактивном режиме, передаем управление
    
            tcsetpgrp(shell_terminal, getpid());
            
            // Восстанавливаем стандартную обработку сигналов 
            signal(SIGINT, SIG_DFL);  
            signal(SIGQUIT, SIG_DFL);  // Ctrl+ '\' 
            signal(SIGTSTP, SIG_DFL);  // Ctrl+Z 
            signal(SIGTTIN, SIG_DFL);  // Чтение с терминала в фоне
            signal(SIGTTOU, SIG_DFL);  // Запись в терминал в фоне
        }

        if (handle_redirection(node->command.redir) != 0) 
            _exit(1);  // Выход с кодом ошибки при проблеме с перенаправлением

        execvp(argv[0], argv);
        
        fprintf(stderr, "%s: command not found\n", argv[0]);
        _exit(127); 

    } else if (pid > 0) {  // Код родительского процесса (shell)
        // помещаем дочерний процесс в его собственную группу
        // вызываем и в родителе, и в ребенке
        setpgid(pid, pid);

        if (shell_is_interactive) {  
            // добавляем команду в список заданий
            
            add_job(pid, argv[0], JOB_RUNNING, 0);

            tcsetpgrp(shell_terminal, pid);
            
            // ждем завершения/приостановки команды
            int rc = wait_foreground_pgid(pid, pid);
            
            //возвращаем управление терминалом shell'у
            tcsetpgrp(shell_terminal, shell_pgid);

            // find_job_by_pgid: ищем задание в списке
            Job *j = find_job_by_pgid(pid);
            if (j && j->status != JOB_STOPPED) {
                j->status = JOB_DONE;  
                delete_job(pid);        
            }

            return rc;  // Возвращаем код возврата команды
            
        } else {  // Неинтерактивный режим
            int status;  
            waitpid(pid, &status, 0);
            
           
            if (WIFEXITED(status)) 
                return WEXITSTATUS(status);  
            
           
            if (WIFSIGNALED(status)) 
                return 128 + WTERMSIG(status); 
            
            return 1;  
        }
        
    } else { 
        perror("fork failed");  
        return 1;  
    }
}
