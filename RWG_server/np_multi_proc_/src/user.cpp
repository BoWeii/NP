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
        usr_broadcast(msg,MSG_NONE);
    }
}

void usr_tell(int uid, const char *msg)
{
    msg_write_msg(msg, MSG_TELL, uid);

    shm_users_wait();
    // Send signal to notify client to read message
    if (shm_users.users[uid].registered)
    {
        int a = kill(shm_users.users[uid].pid, SIGUSR1);
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