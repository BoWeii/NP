#include <iostream>
#include <sys/sem.h>
#include <sys/shm.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "command.h"
#include "user.h"
#include "shell.h"
#include "message.h"
using namespace std;
#define DEFAULT_NAME "(no name)"

shm_users_t shm_users;
int up_fds[MAX_USER_ID + 1];

static void shm_users_wait()
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until w_sem[0] == 0
        {0, 1, SEM_UNDO}, // w_sem[0] += 1
    };

    semop(shm_users.sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void shm_users_signal()
{
    struct sembuf ops[] = {
        {0, -1, SEM_UNDO}, // w_sem[0] -= 1
    };

    semop(shm_users.sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void usr_read_wait(int sem_id)
{
    struct sembuf ops[] = {
        {1, 0, 0},        // wait until read_sem == 0
        {1, 1, SEM_UNDO}, // read_sem += 1
        {3, 0, 0},        // wait until c2_sem == 0
    };

    semop(sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void usr_read_signal(int sem_id)
{
    struct sembuf ops[] = {
        {1, -1, 0}, // read_sem -= 1
        {2, -1, 0}, // c1_sem -= 1
        {3, 1, 0},  // c2_sem += 1
    };

    semop(sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void usr_write_wait(int sem_id)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {0, 1, SEM_UNDO}, // write_sem += 1
        {2, 0, 0},        // wait until c1_sem == 0
    };

    semop(sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void usr_write_signal(int sem_id)
{
    struct sembuf ops[] = {
        {0, -1, 0}, // write_sem -= 1
        {2, 1, 0},  // c1_sem += 1
        {3, -1, 0}, // c2_sem -= 1, if sem_op is less than zero and semval=0, just alter permission without substraction
    };

    semop(sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static int get_unuse_id()
{
    for (int i = 1; i <= MAX_USER_ID; i++)
    {
        if (!shm_users.users[i].registered)
        {
            return i;
        }
    }

    return -1;
}

void users_init()
{
    struct stat st = {0};

    // do not use user[0]
    shm_users.shm_id = shmget(IPC_PRIVATE, sizeof(user_t) * MAX_USER_ID + 1, IPC_CREAT | IPC_EXCL | 0600);
    shm_users.sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0600);

    shm_users.users = (user_t *)shmat(shm_users.shm_id, NULL, 0);

    for (int i = 1; i <= MAX_USER_ID; i++)
    {
        shm_users.users[i].registered = false;
    }

    // user_pipe/<to_uid>/<from_uid>
    if (stat("user_pipe", &st) == -1)
    {
        mkdir("user_pipe", 0700);
    }

    for (int i = 1; i <= MAX_USER_ID; i++)
    {
        char dir_name[16] = {0};
        sprintf(dir_name, "user_pipe/%d", i);
        if (stat(dir_name, &st) == -1)
        {
            mkdir(dir_name, 0700);
        }
    }
}

void users_release()
{
    semctl(shm_users.sem_id, 0, IPC_RMID);
    shmctl(shm_users.shm_id, IPC_RMID, NULL);
}

static void user_sem_init(int sem_id)
{
    struct sembuf ops[] = {
        {3, 1, 0}, // c2_sem += 1
    };

    semop(sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

int usr_add(struct sockaddr_in client_addr)
{
    shm_users_wait();

    int uid = get_unuse_id();

    shm_users.users[uid].id = uid;
    shm_users.users[uid].from_uid = 0;
    shm_users.users[uid].pid = getpid();
    shm_users.users[uid].port = ntohs(client_addr.sin_port);
    shm_users.users[uid].registered = true;
    shm_users.users[uid].sem_id = semget(IPC_PRIVATE, 4, IPC_CREAT | IPC_EXCL | 0600);
    user_sem_init(shm_users.users[uid].sem_id);
    strcpy(shm_users.users[uid].ip, inet_ntoa(client_addr.sin_addr));
    strcpy(shm_users.users[uid].name, DEFAULT_NAME);

    shm_users_signal();

    // Nothing for the new user to read now
    shm_msg.read_offset = shm_msg.msg->write_offset;

    return uid;
}

void usr_remove(int uid)
{
    DIR *dir;
    char buf[0x120] = {0};
    sprintf(buf, "user_pipe/%d", cur_uid);
    dir = opendir(buf);

    shm_users_wait();

    shm_users.users[uid].registered = false;
    semctl(shm_users.users[uid].sem_id, 0, IPC_RMID);

    // Remove FIFO pipe
    if (dir)
    {
        struct dirent *_dirent;
        while ((_dirent = readdir(dir)) != NULL)
        {
            if (_dirent->d_type == DT_FIFO)
            {
                sprintf(buf, "user_pipe/%d/%s", cur_uid, _dirent->d_name);
                unlink(buf);
            }
        }
        closedir(dir);
    }

    shm_users_signal();
}

void usr_broadcast(char *msg, int type)
{
    // Write message
    msg_write_msg(msg, type, 0);

    // Send signal to notify client to read message
    for (int i = 1; i <= MAX_USER_ID; i++)
    {
        shm_users_wait();
        if (shm_users.users[i].registered)
        {
            kill(shm_users.users[i].pid, SIGUSR1);
        }
        shm_users_signal();
    }
}

void usr_who()
{
    char msg[MSG_SIZE_MAX];

    msg_tell(cur_sock_fd, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");

    shm_users_wait();
    for (int i = 1; i <= MAX_USER_ID; i++)
    {
        if (shm_users.users[i].registered)
        {
            sprintf(msg, "%d\t%s\t%s:%d\t",
                    shm_users.users[i].id,
                    shm_users.users[i].name,
                    shm_users.users[i].ip,
                    shm_users.users[i].port);
            msg_tell(cur_sock_fd, string(msg));
            if (cur_uid == shm_users.users[i].id)
            {
                msg_tell(cur_sock_fd, "<-me");
            }

            msg_tell(cur_sock_fd, "\n");
        }
    }
    shm_users_signal();
}

bool usr_find_by_name(string name)
{
    shm_users_wait();
    for (int i = 1; i <= MAX_USER_ID; i++)
    {
        if (shm_users.users[i].registered && !strcmp(shm_users.users[i].name, name.c_str()))
        {
            shm_users_signal();
            return true;
        }
    }
    shm_users_signal();
    return false;
}

void usr_update_name(string name)
{
    char msg[MSG_SIZE_MAX];
    bool is_duplicate = usr_find_by_name(name);

    shm_users_wait();
    if (is_duplicate)
    {
        sprintf(msg, "*** User '%s' already exists. ***\n", name.c_str());
        msg_tell(cur_sock_fd, msg);

        shm_users_signal();
    }
    else
    {
        strcpy(shm_users.users[cur_uid].name, name.c_str());
        sprintf(msg, "*** User from %s:%d is named '%s'. ***\n",
                shm_users.users[cur_uid].ip,
                shm_users.users[cur_uid].port,
                name.c_str());

        shm_users_signal();
        usr_broadcast(msg, MSG_NONE);
    }
}

void usr_tell(int uid, const char *msg)
{
    msg_write_msg(msg, MSG_TELL, uid);

    shm_users_wait();
    // Send signal to notify client to read message
    if (shm_users.users[uid].registered)
    {
        kill(shm_users.users[uid].pid, SIGUSR1);
    }
    shm_users_signal();
}

void usr_builtin_tell(int uid, string msg_str)
{
    char msg[MSG_SIZE_MAX];

    shm_users_wait();

    if (shm_users.users[uid].registered)
    {
        sprintf(msg, "*** %s told you ***: %s\n",
                shm_users.users[cur_uid].name,
                msg_str.c_str());
        shm_users_signal();

        usr_tell(uid, msg);
    }
    else
    {
        shm_users_signal();
        sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", uid);
        msg_tell(cur_sock_fd, msg);
    }
}

void usr_yell(string msg_str)
{
    char msg[MSG_SIZE_MAX];

    shm_users_wait();
    sprintf(msg, "*** %s yelled ***: %s\n", shm_users.users[cur_uid].name, msg_str.c_str());
    shm_users_signal();
    usr_broadcast(msg, MSG_NONE);
}

static int open_dev_null_fd()
{
    return open("/dev/null", O_RDWR);
}

int usr_pipe_to(int to_uid, string cmdline)
{
    // fifo=/user_pipe/{to_uid}/{from_uid}
    char fifo[32];
    char msg[MSG_SIZE_MAX];

    shm_users_wait();

    if (!shm_users.users[to_uid].registered)
    {
        shm_users_signal();

        msg_user_not_exist(to_uid);
        return open_dev_null_fd();
    }

    pid_t to_pid = shm_users.users[to_uid].pid;

    sprintf(fifo, "user_pipe/%d/%d", to_uid, cur_uid);

    if (mkfifo(fifo, 0600) < 0)
    {
        shm_users_signal();

        msg_up_exist(to_uid);
        return open_dev_null_fd();
    }

    // let to_uid knows which pipe need to open
    usr_write_wait(shm_users.users[to_uid].sem_id);
    shm_users.users[to_uid].from_uid = cur_uid;
    usr_write_signal(shm_users.users[to_uid].sem_id);

    sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n",
            shm_users.users[cur_uid].name, cur_uid,
            cmdline.c_str(),
            shm_users.users[to_uid].name, to_uid);

    shm_users_signal();

    // send signal to notify to_uid to open read pipe
    kill(to_pid, SIGUSR2);

    int up_fd = 0;
    if ((up_fd = open(fifo, O_WRONLY)) < 0)
    {
        cerr << "[error] usr_pipe_to fail to open fifo\n";
        return open_dev_null_fd();
    }

    usr_broadcast(msg, MSG_NONE);

    return up_fd;
}

void usr_pipe_open()
{
    // fifo_name=/user_pipe/{to_uid}/{from_uid}
    char fifo[32];
    usr_read_wait(shm_users.users[cur_uid].sem_id);

    // get who pipe to me
    int from_uid = shm_users.users[cur_uid].from_uid;

    sprintf(fifo, "user_pipe/%d/%d", cur_uid, from_uid);

    int up_fd = 0;
    if ((up_fd = open(fifo, O_RDONLY)) < 0)
    {
        cerr << "[error] usr_pipe_open fail to open fifo\n";
    }

    if (up_fds[from_uid])
    {
        // close the previous user pipe
        close(up_fds[from_uid]);
    }

    up_fds[from_uid] = up_fd;

    usr_read_signal(shm_users.users[cur_uid].sem_id);
}

int usr_pipe_from(int from_uid, string cmdline)
{
    // fifo_name=/user_pipe/{to_uid}/{from_uid}
    char fifo[32];
    struct stat st = {0};
    char msg[MSG_SIZE_MAX];

    shm_users_wait();

    if (!shm_users.users[from_uid].registered)
    {
        shm_users_signal();

        msg_user_not_exist(from_uid);
        return open_dev_null_fd();
    }

    sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
            shm_users.users[cur_uid].name, cur_uid,
            shm_users.users[from_uid].name, from_uid,
            cmdline.c_str());

    shm_users_signal();

    sprintf(fifo, "user_pipe/%d/%d", cur_uid, from_uid);
    if (stat(fifo, &st) == -1)
    {
        msg_up_not_exist(from_uid);
        return open_dev_null_fd();
    }

    usr_broadcast(msg, MSG_NONE);
    return up_fds[from_uid];
}

void usr_pipe_release(int from_uid)
{
    // unlink fifo
    // fifoname: user_pipe/<to_uid>/<from_uid>
    char fifo[32];

    sprintf(fifo, "user_pipe/%d/%d", cur_uid, from_uid);

    unlink(fifo);
    
    if (up_fds[from_uid])
    {
        close(up_fds[from_uid]);
        up_fds[from_uid] = 0;
    }
}