/*
 * myshell.c - A simple Unix shell
 * -----------------------------------
 * Supports:
 *   - Running external commands (ls, gcc, python3, etc.)
 *   - Built-in commands: cd, pwd, exit
 *   - Single pipes: command1 | command2
 *   - Background execution with &
 *
 * Compile:  gcc -o myshell myshell.c
 * Run:      ./myshell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

/* ---------- Parsing ---------- */

/*
 * Splits a line of input into an array of argument strings.
 * Example: "ls -la /tmp" -> ["ls", "-la", "/tmp", NULL]
 */
int parse_line(char *line, char **args) {
    int argc = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[argc] = NULL;
    return argc;
}

/* ---------- Built-in commands ---------- */

/* Returns 1 if the command was a built-in (and was handled), 0 otherwise. */
int handle_builtin(char **args, int argc) {
    if (argc == 0) {
        return 1; /* empty line, nothing to do */
    }

    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    if (strcmp(args[0], "cd") == 0) {
        const char *target = (argc > 1) ? args[1] : getenv("HOME");
        if (chdir(target) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd");
        }
        return 1;
    }

    return 0; /* not a built-in */
}

/* ---------- Running external commands ---------- */

/* Runs a single external command and waits for it to finish (unless background). */
void run_command(char **args, int background) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        /* Child process: replace itself with the requested program */
        execvp(args[0], args);
        /* execvp only returns if it failed */
        fprintf(stderr, "myshell: command not found: %s\n", args[0]);
        exit(1);
    } else {
        /* Parent process */
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
        } else {
            printf("[background pid %d]\n", pid);
        }
    }
}

/* Runs two commands connected by a pipe: left | right */
void run_pipeline(char **left_args, char **right_args) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe failed");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        /* First child: writes its output into the pipe */
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        execvp(left_args[0], left_args);
        fprintf(stderr, "myshell: command not found: %s\n", left_args[0]);
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        /* Second child: reads its input from the pipe */
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        execvp(right_args[0], right_args);
        fprintf(stderr, "myshell: command not found: %s\n", right_args[0]);
        exit(1);
    }

    /* Parent closes both ends and waits for both children */
    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

/* ---------- Main loop ---------- */

int main(void) {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        printf("myshell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* Ctrl+D (EOF) was pressed */
            printf("\n");
            break;
        }

        /* Check for background flag (&) */
        int background = 0;
        char *amp = strchr(line, '&');
        if (amp != NULL) {
            background = 1;
            *amp = '\0'; /* remove the & before parsing */
        }

        /* Check for a pipe */
        char *pipe_pos = strchr(line, '|');

        if (pipe_pos != NULL) {
            /* Split the line into left and right commands around the pipe */
            *pipe_pos = '\0';
            char *left_line = line;
            char *right_line = pipe_pos + 1;

            char *left_args[MAX_ARGS];
            char *right_args[MAX_ARGS];
            int left_argc = parse_line(left_line, left_args);
            int right_argc = parse_line(right_line, right_args);

            if (left_argc > 0 && right_argc > 0) {
                run_pipeline(left_args, right_args);
            }
            continue;
        }

        int argc = parse_line(line, args);

        if (handle_builtin(args, argc)) {
            continue;
        }

        if (argc > 0) {
            run_command(args, background);
        }
    }

    return 0;
}
