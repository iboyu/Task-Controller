/* This is the only file you should update and submit. */

/* Fill in your Name and GNumber in the following two comment fields
 * Name: RongLian Yuan
 * GNumber:G01313261
 */

#include <sys/wait.h>
#include "taskctl.h"
#include "parse.h"
#include "util.h"

/* Constants */
#define DEBUG 0

/*
// uncomment if you want to use any of these:

#define NUM_PATHS 2
#define NUM_INSTRUCTIONS 10

static const char *task_path[] = { "./", "/usr/bin/", NULL };
static const char *instructions[] = { "quit", "help", "list", "purge", "exec", "bg", "kill", "suspend", "resume", "pipe", NULL};

*/

/* The entry of your task controller program */

// global variable of node
struct node *my_current = NULL;
int process_ID = 1;
int num_of_node = 0;

// struct of the node
struct node
{
    int my_task_num;
    char *cmd;
    char *instruct;
    char **argv;
    char *in_file;
    char *out_file;
    int status;
    int pid_num;
    int exit_code;
    int fore_or_back;
    struct node *next;
};

// initilize the node of the new process
struct node *new_process(char *my_cmd, char *my_inst, int my_task_num, char *argv[])
{
    struct node *my_node = (struct node *)malloc(sizeof(struct node));
    if (my_node == NULL)
    {
        printf("Malloc failed\n");
        exit(-1);
    }
    my_node->cmd = string_copy(my_cmd);
    my_node->argv = clone_argv(argv);
    my_node->instruct = string_copy(my_inst);
    my_node->in_file = NULL;
    my_node->out_file = NULL;
    my_node->pid_num = 0;
    my_node->exit_code = 0;
    my_node->my_task_num = my_task_num;
    my_node->fore_or_back = -1;
    my_node->status = LOG_STATE_READY;
    my_node->next = NULL;

    return my_node;
}

// insert node
struct node *insert_process_tail(struct node *head, struct node *process, struct node *tail)
{
    struct node *temp = head->next;
    if (temp == NULL)
    {
        head->next = process;
        process_ID++;
    }
    else if (temp->cmd == NULL)
    {
        head->next->cmd = process->cmd;
        head->next->instruct = process->instruct;
        process = head->next;
    }
    else if ((tail->my_task_num) != num_of_node)
    {
        while (temp->cmd != NULL)
        {
            temp = temp->next;
        }
        temp->cmd = process->cmd;
        temp->instruct = process->instruct;
        process = temp;
    }
    else
    {
        while (temp->next != NULL)
        {
            temp = temp->next;
        }
        temp->next = process;
        process_ID++;
    }
    return process;
}

// helper function to return node if found, or return null
struct node *find_process(struct node *head, int task_num)
{
    struct node *current = head->next;
    while (current != NULL)
    {
        if (current->my_task_num == task_num)
        {
            break;
        }
        current = current->next;
    }
    return current;
}

// find the tail of the node
struct node *get_tail_process(struct node *head)
{
    struct node *current = head->next;
    if (current == NULL)
    {
        return NULL;
    }
    while (current->next != NULL)
    {
        current = current->next;
    }
    return current;
}

int child_status = 0;
// my signal handler for SIGCHLD
void my_handler(int sig)
{
    // if(sig == SIGCHLD){

    int my_pid = my_current->pid_num;
    // int ret_pid = 0;
    // do{
    waitpid(-1, &child_status, WNOHANG | WUNTRACED | WCONTINUED);
    //}while(ret_pid >= 0);
    if (WIFEXITED(child_status))
    { // terminated normally
        log_kitc_status_change(my_current->my_task_num, my_pid, my_current->fore_or_back, my_current->cmd, LOG_TERM);
        my_current->exit_code = WEXITSTATUS(child_status);
        my_current->status = LOG_STATE_FINISHED;
    }
    if (WIFSIGNALED(child_status))
    { // terminated by singal
        log_kitc_status_change(my_current->my_task_num, my_pid, my_current->fore_or_back, my_current->cmd, LOG_TERM_SIG);
        my_current->status = LOG_STATE_KILLED;
    }
    if (WIFSTOPPED(child_status))
    { // suspend
        log_kitc_status_change(my_current->my_task_num, my_pid, my_current->fore_or_back, my_current->cmd, LOG_SUSPEND);
        my_current->status = LOG_STATE_SUSPENDED;
    }
    if (WIFCONTINUED(child_status))
    { // continue
        my_current->fore_or_back = LOG_FG;
        log_kitc_status_change(my_current->my_task_num, my_pid, my_current->fore_or_back, my_current->cmd, LOG_RESUME);
        my_current->status = LOG_STATE_RUNNING;
    }
}

// my signal handler for ctrl_c & ctrl_z
void my_handler_two(int sig)
{
    if (my_current != NULL)
    {
        if (my_current->status == LOG_STATE_RUNNING && my_current->fore_or_back == LOG_FG)
        {
            if (sig == SIGTSTP)
            { // Ctrl_z
                log_kitc_ctrl_z();
                kill(my_current->pid_num, SIGTSTP);
            }
            else if (sig == SIGINT)
            { // Ctrl_c
                log_kitc_ctrl_c();
                kill(my_current->pid_num, SIGINT);
            }
        }
    }
}

int main()
{
    char cmdline[MAXLINE]; /* Command line */
    char *cmd = NULL;

    // dummy head
    struct node *head = (struct node *)malloc(sizeof(struct node));
    head->next = NULL;

    /* Intial Prompt and Welcome */
    log_kitc_intro();
    log_kitc_help();

    /* Shell looping here to accept user command and execute */
    while (1)
    {
        char *argv[MAXARGS + 1]; /* Argument list */
        Instruction inst;        /* Instruction structure: check parse.h */

        /* Print prompt */
        log_kitc_prompt();

        /* Read a line */
        // note: fgets will keep the ending '\n'
        errno = 0;
        if (fgets(cmdline, MAXLINE, stdin) == NULL)
        {
            if (errno == EINTR)
            {
                continue;
            }
            exit(-1);
        }

        if (feof(stdin))
        { /* ctrl-d will exit text processor */
            exit(0);
        }

        /* Parse command line */
        if (strlen(cmdline) == 1) /* empty cmd line will be ignored */
            continue;

        cmdline[strlen(cmdline) - 1] = '\0'; /* remove trailing '\n' */

        cmd = malloc(strlen(cmdline) + 1); /* duplicate the command line */
        snprintf(cmd, strlen(cmdline) + 1, "%s", cmdline);

        /* Bail if command is only whitespace */
        if (!is_whitespace(cmd))
        {
            initialize_command(&inst, argv); /* initialize arg lists and instruction */
            parse(cmd, &inst, argv);         /* call provided parse() */

            if (DEBUG)
            { /* display parse result, redefine DEBUG to turn it off */
                debug_print_parse(cmd, &inst, argv, "main (after parse)");
            }

            /* After parsing: your code to continue from here */
            /*================================================*/

            // my signal handler1
            struct sigaction sa = {0};
            sa.sa_handler = my_handler;
            sigaction(SIGCHLD, &sa, NULL);

            // my signal handler2
            struct sigaction sa2;
            memset(&sa2, 0, sizeof(sa2));
            sa2.sa_handler = my_handler_two;
            sigaction(SIGINT, &sa2, NULL);
            sigaction(SIGTSTP, &sa2, NULL);

            if (!strcmp(inst.instruct, "help"))
            {
                log_kitc_help();
            }
            if (!strcmp(inst.instruct, "quit"))
            {
                log_kitc_quit();
                exit(0);
            }

            if (strcmp(inst.instruct, "quit") != 0 && strcmp(inst.instruct, "help") != 0 && strcmp(inst.instruct, "list") != 0 && strcmp(inst.instruct, "purge") != 0 && strcmp(inst.instruct, "exec") != 0 && strcmp(inst.instruct, "bg") != 0 && strcmp(inst.instruct, "kill") != 0 && strcmp(inst.instruct, "suspend") != 0 && strcmp(inst.instruct, "resume") != 0 && strcmp(inst.instruct, "pipe") != 0)
            {

                struct node *my_current_node = new_process(cmd, inst.instruct, process_ID, argv);
                struct node *tail = get_tail_process(head);
                if (num_of_node == 0)
                {
                    head->next = insert_process_tail(head, my_current_node, tail);
                    log_kitc_task_init(head->next->my_task_num, cmd);
                }
                else
                {
                    struct node *current;
                    current = insert_process_tail(head, my_current_node, tail);
                    log_kitc_task_init(current->my_task_num, cmd);
                }
                num_of_node++;
            }
            if (!strcmp(inst.instruct, "list"))
            {
                log_kitc_num_tasks(num_of_node);
                struct node *my_current_node = head->next;
                while (my_current_node != NULL)
                {
                    if (my_current_node->cmd != NULL)
                    {
                        log_kitc_task_info(my_current_node->my_task_num, my_current_node->status, my_current_node->exit_code, my_current_node->pid_num, my_current_node->cmd);
                    }
                    my_current_node = my_current_node->next;
                }
            }
            if (!strcmp(inst.instruct, "purge"))
            {
                struct node *current;
                current = find_process(head, inst.num);
                if (current == NULL)
                {
                    log_kitc_task_num_error(inst.num);
                }
                else if (current->status == LOG_STATE_RUNNING || current->status == LOG_STATE_SUSPENDED)
                {
                    log_kitc_status_error(inst.num, current->status);
                }
                else
                {
                    current->instruct = NULL;
                    current->cmd = NULL;
                    current->exit_code = 0;
                    current->fore_or_back = 0;
                    current->in_file = NULL;
                    current->out_file = NULL;
                    current->pid_num = 0;
                    current->status = 0;
                    num_of_node--;
                    log_kitc_purge(inst.num);
                }
            }

            if (!strcmp(inst.instruct, "exec"))
            {

                struct node *current;
                current = find_process(head, inst.num);

                if (current != NULL)
                {
                    current->in_file = inst.infile;
                    current->out_file = inst.outfile;
                }

                if (current == NULL)
                {
                    log_kitc_task_num_error(inst.num);
                }
                else if (current->status == LOG_STATE_RUNNING || current->status == LOG_STATE_SUSPENDED)
                {
                    log_kitc_status_error(inst.num, current->status);
                }
                else
                {

                    char *path1 = string_copy("./");
                    strcat(path1, current->instruct);
                    char *path2 = string_copy("/usr/bin/");
                    strcat(path2, current->instruct);

                    int pid_num;
                    pid_num = fork();
                    current->pid_num = pid_num;
                    current->status = LOG_STATE_RUNNING;
                    current->fore_or_back = LOG_FG;
                    my_current = current;
                    if (pid_num != 0)
                    { // parent
                        log_kitc_status_change(current->my_task_num, pid_num, current->fore_or_back, current->cmd, LOG_START);
                        wait(&child_status);
                    }
                    else
                    { // child
                        setpgid(0, 0);
                        if (current->in_file != NULL)
                        {
                            log_kitc_redir(current->my_task_num, LOG_REDIR_IN, current->in_file);
                            int input = open(current->in_file, O_RDONLY, 0644);
                            int if_success = dup2(input, STDIN_FILENO);
                            if (if_success == -1)
                            {
                                log_kitc_file_error(current->my_task_num, current->in_file);
                                exit(1);
                            }
                            close(input);
                        }
                        if (current->out_file != NULL)
                        {
                            log_kitc_redir(current->my_task_num, LOG_REDIR_OUT, current->out_file);
                            int output = open(current->out_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
                            int if_success = dup2(output, STDOUT_FILENO);
                            if (if_success == -1)
                            {
                                log_kitc_file_error(current->my_task_num, current->out_file);
                                exit(1);
                            }
                            close(output);
                        }

                        int if_success_path2 = execv(path2, current->argv);
                        int if_success_path1 = execv(path1, current->argv);

                        if (if_success_path1 == -1 && if_success_path2 == -1)
                        {
                            log_kitc_exec_error(current->cmd);
                        }
                        exit(1);
                    }
                }
            }

            if (!strcmp(inst.instruct, "bg"))
            {
                struct node *current;
                current = find_process(head, inst.num);

                if (current != NULL)
                {
                    current->in_file = inst.infile;
                    current->out_file = inst.outfile;
                }

                if (current == NULL)
                {
                    log_kitc_task_num_error(inst.num);
                }
                else if (current->status == LOG_STATE_RUNNING || current->status == LOG_STATE_SUSPENDED)
                {
                    log_kitc_status_error(inst.num, current->status);
                }
                else
                {

                    char *path1 = string_copy("./");
                    strcat(path1, current->instruct);
                    char *path2 = string_copy("/usr/bin/");
                    strcat(path2, current->instruct);

                    int pid_num;
                    pid_num = fork();
                    current->pid_num = pid_num;
                    current->status = LOG_STATE_RUNNING;
                    current->fore_or_back = LOG_BG;
                    my_current = current;

                    if (pid_num != 0)
                    { // parent
                        log_kitc_status_change(current->my_task_num, pid_num, current->fore_or_back, current->cmd, LOG_START);
                    }
                    else
                    { // child
                        setpgid(0, 0);
                        if (current->in_file != NULL)
                        {
                            log_kitc_redir(current->my_task_num, LOG_REDIR_IN, current->in_file);
                            int input = open(current->in_file, O_RDONLY, 0644);
                            int if_success = dup2(input, STDIN_FILENO);
                            if (if_success == -1)
                            {
                                log_kitc_file_error(current->my_task_num, current->in_file);
                                exit(1);
                            }
                            close(input);
                        }
                        if (current->out_file != NULL)
                        {
                            log_kitc_redir(current->my_task_num, LOG_REDIR_OUT, current->out_file);
                            int output = open(current->out_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
                            int if_success = dup2(output, STDOUT_FILENO);
                            if (if_success == -1)
                            {
                                log_kitc_file_error(current->my_task_num, current->out_file);
                                exit(1);
                            }
                            close(output);
                        }

                        int if_success_path2 = execv(path2, current->argv);
                        int if_success_path1 = execv(path1, current->argv);

                        if (if_success_path1 == -1 && if_success_path2 == -1)
                        {
                            log_kitc_exec_error(current->cmd);
                        }
                        exit(1);
                    }
                }
            }

            if (!strcmp(inst.instruct, "kill"))
            {

                struct node *current;
                current = find_process(head, inst.num);
                if (current == NULL)
                {
                    log_kitc_task_num_error(inst.num);
                }
                else if (current->status == LOG_STATE_READY || current->status == LOG_STATE_FINISHED || current->status == LOG_STATE_KILLED)
                {
                    log_kitc_status_error(current->my_task_num, current->status);
                }
                else
                {

                    log_kitc_sig_sent(LOG_CMD_KILL, current->my_task_num, current->pid_num);
                    my_current = current;
                    kill(current->pid_num, SIGINT);
                }
            }

            if (!strcmp(inst.instruct, "suspend"))
            {

                struct node *current;
                current = find_process(head, inst.num);
                if (current == NULL)
                {
                    log_kitc_task_num_error(inst.num);
                }
                else if (current->status == LOG_STATE_READY || current->status == LOG_STATE_FINISHED || current->status == LOG_STATE_KILLED)
                {
                    log_kitc_status_error(current->my_task_num, current->status);
                }
                else
                {
                    log_kitc_sig_sent(LOG_CMD_SUSPEND, current->my_task_num, current->pid_num);

                    my_current = current;
                    kill(current->pid_num, SIGTSTP);
                }
            }

            if (!strcmp(inst.instruct, "resume"))
            {
                struct node *current;
                current = find_process(head, inst.num);
                if (current == NULL)
                {
                    log_kitc_task_num_error(inst.num);
                }
                else if (current->status == LOG_STATE_READY || current->status == LOG_STATE_FINISHED || current->status == LOG_STATE_KILLED)
                {
                    log_kitc_status_error(current->my_task_num, current->status);
                }
                else
                {
                    log_kitc_sig_sent(LOG_CMD_RESUME, current->my_task_num, current->pid_num);
                    my_current = current;
                    kill(current->pid_num, SIGCONT);
                }
            }

            if (!strcmp(inst.instruct, "pipe"))
            {
                int num1 = inst.num;
                int num2 = inst.num2;
                if (num1 == num2)
                {
                    log_kitc_pipe_error(num1);
                }
                else
                {
                    struct node *current1;
                    current1 = find_process(head, num1);
                    struct node *current2;
                    current2 = find_process(head, num2);

                    if (current1 == NULL || current2 == NULL)
                    {
                        if (current1 == NULL)
                        {
                            log_kitc_task_num_error(current1->my_task_num);
                            log_kitc_file_error(current1->my_task_num, LOG_FILE_PIPE);
                        }
                        else if (current2 == NULL)
                        {
                            log_kitc_task_num_error(current2->my_task_num);
                            log_kitc_file_error(current2->my_task_num, LOG_FILE_PIPE);
                        }
                        else
                        {
                            log_kitc_task_num_error(current1->my_task_num);
                            log_kitc_task_num_error(current2->my_task_num);
                            log_kitc_file_error(current1->my_task_num, LOG_FILE_PIPE);
                            log_kitc_file_error(current2->my_task_num, LOG_FILE_PIPE);
                        }
                    }
                    else if (current1->status == LOG_STATE_RUNNING || current1->status == LOG_STATE_SUSPENDED || current2->status == LOG_STATE_RUNNING || current2->status == LOG_STATE_SUSPENDED)
                    {
                        if (current1->status == LOG_STATE_RUNNING || current1->status == LOG_STATE_SUSPENDED)
                        {
                            log_kitc_status_error(current1->my_task_num, current1->status);
                            log_kitc_file_error(current1->my_task_num, LOG_FILE_PIPE);
                        }
                        else if (current2->status == LOG_STATE_RUNNING || current2->status == LOG_STATE_SUSPENDED)
                        {
                            log_kitc_status_error(current2->my_task_num, current2->status);
                            log_kitc_file_error(current2->my_task_num, LOG_FILE_PIPE);
                        }
                    }
                    else
                    {
                        // set the pipe
                        int ary[2] = {0};
                        pipe(ary);
                        int fd_read = ary[0];
                        int fd_write = ary[1];

                        // set the path1 and path2 for current1
                        char *path1 = string_copy("./");
                        strcat(path1, current1->instruct);
                        char *path2 = string_copy("/usr/bin/");
                        strcat(path2, current1->instruct);

                        // background process run for task_num 1
                        int pid_num_task1;
                        pid_num_task1 = fork();
                        current1->pid_num = pid_num_task1;
                        current1->status = LOG_STATE_RUNNING;
                        current1->fore_or_back = LOG_BG;
                        my_current = current1;

                        if (pid_num_task1 != 0)
                        { // parent
                            log_kitc_status_change(current1->my_task_num, pid_num_task1, current1->fore_or_back, current1->cmd, LOG_START);
                        }
                        else
                        { // child
                            setpgid(0, 0);
                            close(fd_read);
                            dup2(fd_write, STDOUT_FILENO);
                            int if_success_path2 = execv(path2, current1->argv);
                            int if_success_path1 = execv(path1, current1->argv);
                            if (if_success_path1 == -1 && if_success_path2 == -1)
                            {
                                log_kitc_exec_error(current1->cmd);
                            }
                            close(fd_write);
                            exit(0);
                        }

                        // set the path1 and path2 for current2
                        char *path1_1 = string_copy("./");
                        strcat(path1_1, current2->instruct);
                        char *path2_2 = string_copy("/usr/bin/");
                        strcat(path2_2, current2->instruct);

                        // foreground process run for task 2
                        int pid_num_task2;
                        pid_num_task2 = fork();
                        current2->pid_num = pid_num_task2;
                        current2->status = LOG_STATE_RUNNING;
                        current2->fore_or_back = LOG_FG;
                        my_current = current2;

                        if (pid_num_task2 != 0)
                        { // parent
                            log_kitc_status_change(current2->my_task_num, pid_num_task2, current2->fore_or_back, current2->cmd, LOG_START);
                            wait(&child_status);
                        }
                        else
                        { // child
                            setpgid(0, 0);
                            close(fd_write);
                            dup2(fd_read, STDIN_FILENO);
                            int if_success_path2 = execv(path2_2, current2->argv);
                            int if_success_path1 = execv(path1_1, current2->argv);
                            if (if_success_path1 == -1 && if_success_path2 == -1)
                            {
                                log_kitc_exec_error(current2->cmd);
                            }
                            close(fd_read);
                            exit(0);
                        }
                        close(fd_write);
                        close(fd_read);
                        waitpid(-1, NULL, 0);
                        waitpid(-1, NULL, 0);
                    }
                }
            }

        } // end if(!is_whitespace(cmd))

        free(cmd);
        cmd = NULL;
        free_command(&inst, argv);
    } // end while(1)

    return 0;
} // end main()
