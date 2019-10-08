#include <stdio.h>
#include <curses.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include "demofuncs.h"

#define BUFFSIZE 100
#define PROC_INPUT 1
#define PROC_ESC 2
#define PROC_ESC_INPUT 3
#define PROC_F 4
#define PROC_G 5

#define NORMAL 10
#define FASTER_F 11
#define FASTER_G 12
#define TERMINATED_ALL 13
#define TERMINATED_F 14
#define TERMINATED_G 15

typedef struct TProcess
{
    pid_t pid;      //process id
    int fd[2];      //pipe
} *PTProcess;

struct TAnswer
{
    int error_type;
    bool f_value, ended_f;
    bool g_value, ended_g;
};

bool f(int x)
{
    return f_func_and(x);
}

bool g(int x)
{
    return g_func_and(x);
}

bool read_from_pipe(int file)
{
    char buff[BUFFSIZE];
    read(file, buff, sizeof(buff));
    bool x;
    sscanf(buff, "%d", &x);
    return x;
}

void write_to_pipe(int file, bool val)
{
    char buff[BUFFSIZE];
    sprintf(buff,"%d", val);
    write(file, buff, strlen(buff)+1);
    return;
}

void check_escape(PTProcess parent, int value)
{
    initscr();
    printw("Value x is %d.\nClick on Esc to terminate a process.\n", value);
    refresh();
    keypad(stdscr,TRUE);
    noecho();

    fd_set read_fds;
    int return_val;
    char c = 0;

    do{
        FD_ZERO(&read_fds);
        FD_SET(0, &read_fds);
        FD_SET(parent->fd[0], &read_fds);
        return_val = select(FD_SETSIZE, &read_fds, NULL, NULL, NULL);
        if (return_val == -1)
        {
            endwin();
            exit(EXIT_FAILURE);
        }
        else if (return_val){
            if (FD_ISSET(0, &read_fds))
                c = getch();
            else {
                read_from_pipe(parent->fd[0]);
                break;
            }
        }
    }
    while (c != 27);
    endwin();
    return;
}


void start_process(PTProcess parent, PTProcess child, int proc_type, int value);

bool user_input_terminate()
{
    bool ok = true;
    char c;
    do {
        c = getchar();
        if (c == 'y') {
            ok = true;
            break;
        } else if (c == 'n') {
            ok = false;
            break;
        }
    }
    while (true);
    return ok;
}

bool user_terminate(PTProcess parent)
{
    initscr();
    fd_set read_fds;
    int return_val;
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    char c = 0;
    struct TProcess child;
    start_process(NULL, &child, PROC_ESC_INPUT, 0);
    bool ok = true;
    int counter = 60;
    printw("Would you like to terminate process? y/n\n");
    refresh();
    do{
        printw("\r");
        if (counter < 10)
            printw(" ");
        printw("%d sec. left ", counter);
        refresh();
        FD_ZERO(&read_fds);
        FD_SET(child.fd[0], &read_fds);
        FD_SET(parent->fd[0], &read_fds);
        timeout.tv_sec = 1;
        return_val = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);
        if (return_val == -1) {
            endwin();
            exit(EXIT_FAILURE);
        } else if (return_val) {
            if (FD_ISSET(child.fd[0], &read_fds)) {
                ok = read_from_pipe(child.fd[0]);
                break;
            } else {
                read_from_pipe(parent->fd[0]);
                break;
            }
        } else {
            counter -= 1;
        }
    }
    while (counter != 0);
    kill(child.pid, SIGKILL);

    endwin();
    return ok;
}

void start_process(PTProcess parent, PTProcess child, int proc_type, int value)
{
    if (pipe(child->fd) < 0)
        exit(1);
    child->pid = fork();
    if (child->pid < 0)
        exit(EXIT_FAILURE);
    else if (child->pid == 0) {
        close(child->fd[0]);
        bool ans = false;
        if (proc_type == PROC_INPUT) {
            check_escape(parent, value);
        } else if (proc_type == PROC_F) {
            ans = f(value);
        } else if (proc_type == PROC_G) {
            ans = g(value);
        } else if (proc_type == PROC_ESC) {
            ans = user_terminate(parent);
        }
        else if (proc_type == PROC_ESC_INPUT)
            ans = user_input_terminate();
        write_to_pipe(child->fd[1], ans);
        close(child->fd[1]);
        exit(EXIT_SUCCESS);
    }
}

void kill_input_proc(PTProcess proc_main, PTProcess proc_input)
{
    close(proc_main->fd[0]);
    write_to_pipe(proc_main->fd[1], true);
    close(proc_main->fd[1]);
    usleep(1000);
    kill(proc_input->pid, SIGKILL);
    return;
}

void kill_processes(struct TAnswer *ans, PTProcess proc_main, PTProcess proc_input, PTProcess proc_f, PTProcess proc_g)
{
    if (!ans->ended_f && !ans->ended_g)
    {
        ans->error_type = TERMINATED_ALL;
        kill(proc_f->pid, SIGKILL);
        kill(proc_g->pid, SIGKILL);
    }
    else {
        if (ans->ended_g && ans->ended_f) {
            ans->error_type = NORMAL;
            kill_input_proc(proc_main, proc_input);
        }
        else if (ans->f_value && ans->g_value)
        {
            if (ans->ended_f) {
                ans->error_type = TERMINATED_G;
                kill(proc_g->pid, SIGKILL);
            }
            else {
                ans->error_type = TERMINATED_F;
                kill(proc_f->pid, SIGKILL);
            }
        }
        else if (ans->ended_f) {
            ans->error_type = FASTER_F;
            kill_input_proc(proc_main, proc_input);
            kill(proc_g->pid, SIGKILL);
        }
        else if (ans->ended_g) {
            ans->error_type = FASTER_G;
            kill_input_proc(proc_main, proc_input);
            kill(proc_f->pid, SIGKILL);
        }
    }
}

struct TAnswer set_select(PTProcess proc_main, PTProcess proc_input, PTProcess proc_f, PTProcess proc_g, int value_x)
{
    fd_set read_fds;
    int return_val;
    bool esc = false;
    struct TAnswer ans;
    ans.ended_f = ans.ended_g = false;
    ans.f_value = ans.g_value = true;
    while(true)
    {
        FD_ZERO(&read_fds);
        FD_SET(proc_input->fd[0], &read_fds);
        FD_SET(proc_f->fd[0], &read_fds);
        FD_SET(proc_g->fd[0], &read_fds);
        return_val = select(FD_SETSIZE, &read_fds, NULL, NULL, NULL);
        if (return_val == -1){
            exit(EXIT_FAILURE);
        }
        else if (return_val) {
            if (FD_ISSET(proc_f->fd[0], &read_fds)) {
                ans.f_value = read_from_pipe(proc_f->fd[0]);
                ans.ended_f = true;
                if (ans.ended_g || !ans.f_value)
                    break;
            } else if (FD_ISSET(proc_g->fd[0], &read_fds)) {
                ans.g_value = read_from_pipe(proc_g->fd[0]);
                ans.ended_g = true;
                if (ans.ended_f || !ans.g_value)
                    break;
            } else if (FD_ISSET(proc_input->fd[0], &read_fds)) {
                if (!esc) {
                    esc = true;
                    bool temp = read_from_pipe(proc_input->fd[0]);
                    start_process(proc_main, proc_input, PROC_ESC, 0);
                } else {
                    esc = false;
                    bool terminate = read_from_pipe(proc_input->fd[0]);
                    if (terminate)
                        break;
                    else
                        start_process(proc_main, proc_input, PROC_INPUT, value_x);
                }
            }
        } else {
            break;
        }
    }

    kill_processes(&ans, proc_main, proc_input, proc_f, proc_g);
    return ans;
}

int work_with_user()
{
    int value_x;
    printf("Input x: ");
    scanf("%d", &value_x);

    struct TProcess proc_main, proc_input, proc_f, proc_g;
    if (pipe(proc_main.fd) < 0)
        return 1;
    start_process(&proc_main, &proc_input, PROC_INPUT, value_x);
    start_process(NULL, &proc_f, PROC_F, value_x);
    start_process(NULL, &proc_g, PROC_G, value_x);

    struct TAnswer answer = set_select(&proc_main, &proc_input, &proc_f, &proc_g, value_x);
    bool temp;
    switch (answer.error_type)
    {
        case NORMAL:
            temp = answer.f_value && answer.g_value;
            printf("Counted f = %d and g = %d. Answer is %d.\n", answer.f_value, answer.g_value, temp);
            break;
        case FASTER_F:
            printf("Function f was counted faster and got zero value. Answer is %d.\n", answer.f_value);
            break;
        case FASTER_G:
            printf("Function g was counted faster and got zero value. Answer is %d.\n", answer.g_value);
            break;
        case TERMINATED_ALL:
            printf("Function f and g wasn't counted.\n");
            break;
        case TERMINATED_F:
            printf("Function g was counted(f not) and got non-zero value. g is %d. Answer cannot be determined\n", answer.g_value);
            break;
        case TERMINATED_G:
            printf("Function f was counted(g not) and got non-zero value. f is %d. Answer cannot be determined\n", answer.f_value);
            break;
    }
    return 0;
}

int main()
{
    return work_with_user();
}

