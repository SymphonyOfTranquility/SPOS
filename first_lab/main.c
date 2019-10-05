#include <stdio.h>
//#include <curses.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

#define BUFFSIZE 100

struct TProcess
{
    pid_t pid;      //process id
    int fd[2];      //pipe
};

struct TAnswer
{
    bool value;
    int error_type;
};

bool f(int x)
{
    return (x*x)%2 == 0;
}

bool g(int x)
{
    return (x+1)%2 == 0;
}

int read_from_pipe(int file)
{
    char buff[BUFFSIZE];
    read(file, buff, sizeof(buff));
    int x;
    sscanf(buff, "%d", &x);
    return x;
}

void write_to_pipe(int file, int val)
{
    char buff[BUFFSIZE];
    sprintf(buff,"%d", val);
    write(file, buff, strlen(buff)+1);
    return;
}

bool check_esc()
{
    /*initscr();
    keypad(stdscr,TRUE);
    noecho();
    char c = 0;
    do
        c = getch();
    while (c != 27);
    return 0;
    printf("Click on Esc to terminate a process.\n");
    int c;
    scanf("%d", &c);*/
    sleep(1);
    return true;
}

void start_process(struct TProcess *child, int k, int val)
{
    if (pipe(child->fd) < 0)
        exit(1);
    child->pid = fork();
    if (child->pid < 0)
    {
        exit(EXIT_FAILURE);
    }
    else if (child->pid  == 0)
    {
        printf("%d\n",k);
        close(child->fd[0]);
        bool ans;
        if (k == 1)
        {
            ans = check_esc();
        }
        else if (k == 2)
        {
            sleep(10);
            ans = f(val);
        }
        else if (k == 3)
        {
            sleep(12);
            ans = g(val);
        }
        write_to_pipe(child->fd[1], (int)ans);
        close(child->fd[1]);
        printf("child_proc1 %d %d\n", k, ans);
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf("%d parent\n", child->pid);
    }
}

struct TAnswer set_select(struct TProcess *proc_input, struct TProcess *proc_f, struct TProcess *proc_g)
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    FD_ZERO(&rfds);
    FD_SET(proc_input->fd[0], &rfds);
    FD_SET(proc_f->fd[0], &rfds);
    FD_SET(proc_g->fd[0], &rfds);
    tv.tv_sec = 2;
    int counter = 0;
    bool esc = false;
    struct TAnswer ans;
    bool f_value, g_value;
    while(true)
    {
        printf("a\n");
        if (!esc)
            retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
        else
            retval = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

        printf("ret = %d\n", retval);
        if (retval == -1) {
            printf("error  select()\n");
        }
        else if (retval)
        {
            if (FD_ISSET(proc_input->fd[0], &rfds)) {
                printf("inp\n");
                esc = true;
                tv.tv_sec = 2;
                FD_ZERO(&rfds);
                FD_SET(proc_f->fd[0], &rfds);
                FD_SET(proc_g->fd[0], &rfds);
                //TO DO
            }
            else if (FD_ISSET(proc_f->fd[0], &rfds))
            {
                ++counter;
                f_value = (bool)read_from_pipe(proc_f->fd[0]);
                if (counter == 2)
                    break;
                FD_ZERO(&rfds);
                FD_SET(proc_input->fd[0], &rfds);
                FD_SET(proc_g->fd[0], &rfds);
                if (f_value)
                {
                    kill(proc_input->pid, SIGKILL);
                    kill(proc_g->pid, SIGKILL);
                    ans.error_type = 1;
                    ans.value = f_value;
                    return ans;
                }
            }
            else if (FD_ISSET(proc_g->fd[0], &rfds))
            {
                ++counter;
                g_value = (bool)read_from_pipe(proc_g->fd[0]);
                if (counter == 2)
                    break;
                FD_ZERO(&rfds);
                FD_SET(proc_input->fd[0], &rfds);
                FD_SET(proc_f->fd[0], &rfds);
                if (g_value)
                {
                    kill(proc_input->pid, SIGKILL);
                    kill(proc_f->pid, SIGKILL);
                    ans.error_type = 2;
                    ans.value = g_value;
                    return ans;
                }
            }
        } else {
            printf("FFF\n");
            FD_ZERO(&rfds);
            FD_SET(proc_input->fd[0], &rfds);
            FD_SET(proc_f->fd[0], &rfds);
            FD_SET(proc_g->fd[0], &rfds);
            tv.tv_sec = 2;
        }
    }
    printf("uuuu %d\n", counter);
    if (counter == 2)
    {
        ans.value = f_value || g_value;
        ans.error_type = 0;
        kill(proc_input->pid, SIGKILL);
        return ans;
    }
}

int main()
{
    int variable;
    printf("Input x: ");
    scanf("%d", &variable);
//    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    struct TProcess proc_input, proc_f, proc_g;
    start_process(&proc_input, 1, variable);
    start_process(&proc_f, 2, variable);
    start_process(&proc_g, 3, variable);

    struct TAnswer ans = set_select(&proc_input, &proc_f, &proc_g);
    for (int i = 0;i < 3; ++i) {
        int status;
        int corpse = wait(&status);
        if (corpse == proc_f.pid) {
            printf("F\n");
        } else if (corpse == proc_g.pid) {
            printf("G\n");
        } else
            printf("asrerarer\n");
    }
    printf("%d %d\n", ans.value, ans.error_type);
    return 0;
}

