#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFSIZE 100

int f(int x)
{
    return x*x;
}

int g(int x)
{
    return x+1;
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

int start_process(int child_p[2], int k, int val)
{
    pid_t pid;
    pid = fork();
    if (pid < 0)
    {
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        printf("%d\n",k);
        close(child_p[0]);
        int ans;
        if (k == 1)
            ans = f(val);
        else
            ans = g(val);

        write_to_pipe(child_p[1], ans);
        close(child_p[1]);
        printf("child_proc1 %d \n", k);
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf("%d parent\n", pid);
        return pid;
    }
}

void set_select(int main_pipe[2], int f_pipe[2], int g_pipe[2])
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    retval = select(1, &rfds, NULL, NULL, &tv);
    if (retval == -1)
        perror("select()");
    else if (retval)
        printf("dkfrg\n");
    else
        printf("FFF\n");
}

int main()
{
    int variable;
    printf("Input x: ");
    scanf("%d", &variable);
//    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    int m_pipe[2], f_pipe[2], g_pipe[2];
    if (pipe(m_pipe) < 0)
        exit(1);
    if (pipe(f_pipe) < 0)
        exit(1);
    if (pipe(g_pipe) < 0)
        exit(1);

    int pid_f = start_process(f_pipe, 1, variable);
    int pid_g = start_process(g_pipe, 2, variable);

    printf("Click on Esc to terminate a process.\n");
    set_select(m_pipe, f_pipe, g_pipe);

    for (int i = 0;i < 2;  ++i)
    {
        int status;
        int corpse = wait(&status);
        if (corpse == pid_f)
        {
            int ans_f = read_from_pipe(f_pipe[0]);
            printf("pid F %d %d\n", corpse, ans_f);
        }
        else
        {
            int ans_g = read_from_pipe(g_pipe[0]);
            printf("pid G %d %d\n", corpse, ans_g);
        }
    }

    return 0;
}

