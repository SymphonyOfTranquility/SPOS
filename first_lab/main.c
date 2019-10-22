#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <stdlib.h>
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

typedef struct TProcess
{
    pid_t pid;      //process id
    int fd[2];      //pipe
} *PTProcess;

struct TAnswer
{
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
    int temp;
    bool x;
    sscanf(buff, "%d", &temp);
    x = (bool)temp;
    return x;
}

void write_to_pipe(int file, bool val)
{
    char buff[BUFFSIZE];
    int temp = val;
    sprintf(buff,"%d", temp);
    write(file, buff, strlen(buff)+1);
    return;
}

void check_escape(PTProcess parent, int value, WINDOW* work_window)
{
    clear();
    wprintw(work_window, "Value x is %d.\nComputing.\nClick on Esc to terminate a process.\n", value);
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
                c = wgetch(work_window);
            else {
                read_from_pipe(parent->fd[0]);
                break;
            }
        }
    }
    while (c != 27);
    return;
}


void start_process(PTProcess parent, PTProcess child, int proc_type, int value, WINDOW* work_window);

bool user_input_terminate(WINDOW* work_window)
{
    bool ok = true;
    char c;
    noecho();
    do {
        c = wgetch(work_window);
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

bool user_terminate(PTProcess parent, WINDOW* work_window)
{
    clear();
    wrefresh(work_window);
    int h, w;
    getmaxyx(work_window, h, w);
    WINDOW* term_window = derwin(work_window, 4, 44, h/2-2, w/2-22);
    box( term_window, ACS_VLINE, ACS_HLINE );
    wrefresh(term_window);


    fd_set read_fds;
    int return_val;
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    char c = 0;
    struct TProcess child;
    start_process(NULL, &child, PROC_ESC_INPUT, 0, term_window);
    bool ok = true;
    int counter = 60;
    wmove(term_window, 1, 1);
    wprintw(term_window, "Would you like to terminate process? y/n");
    wmove(term_window, 2, 1);
    wprintw(term_window, "%d sec. left ", counter);
    wrefresh(term_window);
    do{
        wmove(term_window, 2, 1);
        if (counter < 10)
            wprintw(term_window, " ");
        wprintw(term_window, "%d", counter);
        wmove(term_window, 2, 2);
        wrefresh(term_window);
        FD_ZERO(&read_fds);
        FD_SET(child.fd[0], &read_fds);
        FD_SET(parent->fd[0], &read_fds);
        timeout.tv_sec = 1;
        return_val = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);
        if (return_val == -1) {
            wstandend(term_window);
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
    wstandend(term_window);
    return ok;
}

void start_process(PTProcess parent, PTProcess child, int proc_type, int value, WINDOW* work_window)
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
            check_escape(parent, value, work_window);
        } else if (proc_type == PROC_F) {
            ans = f(value);
        } else if (proc_type == PROC_G) {
            ans = g(value);
        } else if (proc_type == PROC_ESC) {
            ans = user_terminate(parent, work_window);
        }
        else if (proc_type == PROC_ESC_INPUT)
            ans = user_input_terminate(work_window);
        write_to_pipe(child->fd[1], ans);
        close(child->fd[1]);
        exit(EXIT_SUCCESS);
    }
}


void check_to_kill_proc(PTProcess proc)
{
    int status;
    pid_t return_pid = waitpid(proc->pid, &status, WNOHANG);
    if (return_pid == 0)
        kill(proc->pid, SIGKILL);
    return;
}

void kill_input_proc(PTProcess proc_main, PTProcess proc_input)
{
    close(proc_main->fd[0]);
    write_to_pipe(proc_main->fd[1], true);
    close(proc_main->fd[1]);
    int status;
    pid_t return_pid = 0;
    while (return_pid == 0)
        return_pid = waitpid(proc_input->pid, &status, WNOHANG);
    return;
}


void output_ans(struct TAnswer *ans, WINDOW* work_window)
{
    if (!ans->ended_f && !ans->ended_g)
        wprintw(work_window, "Answer is UNDEFINED, because f and g weren't counted.\n");
    else {
        if (ans->ended_g && ans->ended_f)
            wprintw(work_window, "Answer is %d.\n", ans->g_value && ans->f_value);
        else if (ans->f_value && ans->g_value)
        {
            if (ans->ended_f) {
                wprintw(work_window, "Answer is UNDEFINED, because g wasn't counted.\n");
            }
            else {
                wprintw(work_window, "Answer is UNDEFINED, because f wasn't counted.\n");
            }
        }
        else if (ans->ended_f)
            wprintw(work_window, "Answer is %d.\n", ans->f_value);
        else if (ans->ended_g)
            wprintw(work_window, "Answer is %d.\n", ans->g_value);
    }
}

struct TAnswer set_select(PTProcess proc_main,
        PTProcess proc_input,
        PTProcess proc_f,
        PTProcess proc_g,
        int value_x,
        WINDOW* work_window)
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
                    start_process(proc_main, proc_input, PROC_ESC, 0, work_window);
                } else {
                    esc = false;
                    bool terminate = read_from_pipe(proc_input->fd[0]);
                    if (terminate)
                        break;
                    else
                        start_process(proc_main, proc_input, PROC_INPUT, value_x, work_window);
                }
            }
        } else {
            break;
        }
    }    
    return ans;
}

int work_with_user()
{
    int value_x;
    WINDOW* work_window = initscr();
    wprintw(work_window, "Input x:\n");
    while (true)
    {
        wscanw(work_window, "%d", &value_x);
        if (value_x == -1)
            break;

        struct TProcess proc_main, proc_input, proc_f, proc_g;
        if (pipe(proc_main.fd) < 0)
            return 1;
        start_process(&proc_main, &proc_input, PROC_INPUT, value_x, work_window);
        start_process(NULL, &proc_f, PROC_F, value_x, NULL);
        start_process(NULL, &proc_g, PROC_G, value_x, NULL);

        struct TAnswer answer = set_select(&proc_main, &proc_input, &proc_f, &proc_g, value_x, work_window);
        
        check_to_kill_proc(&proc_f);
        check_to_kill_proc(&proc_g);
        kill_input_proc(&proc_main, &proc_input);
        
        bool temp;
        clear();
        wprintw(work_window, "Value x is %d.\n", value_x);
        output_ans(&answer, work_window);

        wprintw(work_window, "\nInput new x (to exit input -1):\n");
    }
    endwin();
    return 0;
}

int main()
{
    return work_with_user();
}

