#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>

#include "message.h"
#include "sock.h"
#include "user.h"
#include "shell.h"

using namespace std;

shm_msg_t shm_msg;

static void msg_read_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {1, 1, SEM_UNDO}, // read_sem += 1
    };

    semop(shm_msg.sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_read_signal(void)
{
    struct sembuf ops[] = {
        {1, -1, SEM_UNDO}, // read_sem -= 1
    };

    semop(shm_msg.sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_write_wait(void)
{
    struct sembuf ops[] = {
        {0, 0, 0},        // wait until write_sem == 0
        {0, 1, SEM_UNDO}, // write_sem += 1
        {1, 0, 0},        // wait until read_sem == 0
    };

    semop(shm_msg.sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

static void msg_write_signal(void)
{
    struct sembuf ops[] = {
        {0, -1, SEM_UNDO}, // write_sem -= 1
    };

    semop(shm_msg.sem_id, ops, sizeof(ops) / sizeof(ops[0]));
}

void msg_welcome(int sock_fd)
{
    string welcome = "****************************************\n"
                     "** Welcome to the information server. **\n"
                     "****************************************\n";
    msg_tell(sock_fd, welcome);
}

void msg_init()
{
    shm_msg.shm_id = shmget(IPC_PRIVATE, sizeof(message_t), IPC_CREAT | IPC_EXCL | 0600);
    shm_msg.msg = (message_t *)shmat(shm_msg.shm_id, NULL, 0);
    shm_msg.msg->write_offset = shm_msg.read_offset = 0;
    shm_msg.msg->mem[0] = 0;
    shm_msg.sem_id = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600);
}

void msg_release()
{
    semctl(shm_msg.sem_id, 0, IPC_RMID);
    shmctl(shm_msg.shm_id, IPC_RMID, NULL);
}

void msg_tell(int sock_fd, string msg)
{
    sock_write(sock_fd, msg.c_str(), msg.size());
}

void msg_prompt(int sock_fd)
{
    const char _prompt[] = "% ";
    sock_write(sock_fd, _prompt, 2);
}

void msg_write_msg(const char *content, int type, int arg)
{

    char *msg = (char *)malloc(sizeof(char) * (strlen(content) + 0x10));

    switch (type)
    {
    case MSG_TELL:
        // "MSG_TELL <to_uid> <content><MSG_STR_DELIM>"
        sprintf(msg, "%d %d %s%c", MSG_TELL, arg, content, MSG_STR_DELIM);
        break;

    case MSG_LOGOUT:
        // "MSG_LOGOUT <logout_uid> <content><MSG_STR_DELIM>"
        sprintf(msg, "%d %d %s%c", MSG_LOGOUT, cur_uid, content, MSG_STR_DELIM);
        break;
    default:
        // "MSG_NONE <content><MSG_STR_DELIM>"
        sprintf(msg, "%d %s%c", MSG_NONE, content, MSG_STR_DELIM);
        break;
    }

    int msg_len = strlen(msg);

    msg_write_wait();

    char *mem = shm_msg.msg->mem;
    int *write_offset_ptr = &(shm_msg.msg->write_offset);
    int left_len = MSG_SIZE_MAX - *write_offset_ptr - 1; // 1 for NULL byte for ring buffer
    message_str_t *ptr = (message_str_t *)(&mem[*write_offset_ptr]);

    // 1 for NULL byte for str, 1 for MSG_CANT_EXCEED
    if (left_len >= msg_len + 1 + 1)
    {
        ptr->tag = MSG_CANT_EXCEED;
        strncpy(ptr->content, msg, msg_len);
        ptr->content[msg_len] = 0;

        *write_offset_ptr += msg_len + 1 + 1;
        *write_offset_ptr %= (MSG_SIZE_MAX - 1);
    }
    else
    {
        int remain_len = left_len - 1; // 1 for MSG_CANT_EXCEED
        ptr->tag = MSG_CANT_EXCEED;
        strncpy(ptr->content, msg, remain_len);

        if (msg_len != remain_len)
        {
            // 4 5 0 _ E 1 2 3 N
            // _ _ _ _ _ _ _ _ _

            strncpy(mem, &msg[remain_len], msg_len - remain_len);
            mem[msg_len - remain_len] = 0;

            *write_offset_ptr = msg_len - remain_len + 1; // 1 for NULL byte for str
        }
        else
        {
            // _ _ _ E 1 2 3 N
            // _ _ _ _ _ _ _ _
            *write_offset_ptr = 0;
        }
    }

    msg_write_signal();

    free(msg);
}

void msg_read_msg()
{
    msg_read_wait();

    char *mem = shm_msg.msg->mem;
    int write_offset = shm_msg.msg->write_offset;
    int *read_offset_ptr = (int *)(&shm_msg.read_offset);
    int read_offset = *read_offset_ptr;
    char *content;

    message_str_t *ptr;
    if ((unsigned char)mem[*read_offset_ptr] != MSG_CANT_EXCEED)
    {
        // Lose some message
        // Reset read_offset
        *read_offset_ptr = write_offset;

        msg_read_signal();
        return;
    }

    // get msg_len
    ptr = (message_str_t *)&(mem[*read_offset_ptr]);
    content = ptr->content;

    int msg_len = 0;
    while (*read_offset_ptr != write_offset)
    {
        int len = strlen(content);
        msg_len += len;

        *read_offset_ptr += len + 1; // 1 for NULL byte
        *read_offset_ptr %= MSG_SIZE_MAX;

        if (*read_offset_ptr + 1 == MSG_SIZE_MAX)
        {
            // check the NULL byte for ring buffer
            *read_offset_ptr = 0;
        }
        content = &mem[*read_offset_ptr];
    }

    // read message
    if (msg_len)
    {
        char *msg = (char *)malloc(sizeof(char) * (msg_len + 1));
        char delims[] = {MSG_STR_DELIM, 0};

        ptr = (message_str_t *)&(mem[read_offset]);
        content = ptr->content;

        int idx = 0;
        while (read_offset != write_offset)
        {
            int len = strlen(content);
            strcpy(&msg[idx], content);
            idx += len;

            read_offset += len + 1; // 1 for NULL byte
            read_offset %= MSG_SIZE_MAX;

            if (read_offset + 1 == MSG_SIZE_MAX)
            {
                // check the NULL byte for ring buffer
                read_offset = 0;
            }
            content = &mem[read_offset];
        }

        msg_read_signal();

        char *token, *token_end;
        token = strtok_r(msg, delims, &token_end);
        if (!token)
        {
            free(msg);
            return;
        }

        while (token)
        {
            char *sub_token, *sub_token_end;
            sub_token = strtok_r(token, " ", &sub_token_end);

            if (!sub_token)
            {
                free(msg);
                return;
            }

            if ((unsigned char)sub_token[0] == MSG_CANT_EXCEED)
            {
                sub_token += 1;
            }

            int msg_type = atoi(sub_token);
            int who;
            switch (msg_type)
            {
            case MSG_TELL:
                // "MSG_TELL <to_uid> <content><MSG_STR_DELIM>"
                sub_token = strtok_r(NULL, " ", &sub_token_end);
                who = atoi(sub_token);

                if (who == cur_uid)
                {
                    sub_token = sub_token + strlen(sub_token) + 1;
                    msg_tell(cur_sock_fd, sub_token);
                }
                break;

            case MSG_LOGOUT:
                // "MSG_LOGOUT <logout_uid> <content><MSG_STR_DELIM>"
                sub_token = strtok_r(NULL, " ", &sub_token_end);
                who = atoi(sub_token);
                usr_pipe_release(who);

                sub_token = sub_token + strlen(sub_token) + 1;
                msg_tell(cur_sock_fd, sub_token);
                break;
            default:
                // "MSG_NONE <content><MSG_STR_DELIM>"
                sub_token = sub_token + strlen(sub_token) + 1;
                msg_tell(cur_sock_fd, sub_token);
                break;
            }

            token = strtok_r(NULL, delims, &token_end);
        }
        free(msg);
        return;
    }

    msg_read_signal();
    return;
}

void msg_user_not_exist(int uid)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", uid);
    msg_tell(cur_sock_fd, string(msg));
}

void msg_up_exist(int to_uid)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** Error: the pipe #%d->#%d already exists. ***\n", cur_uid, to_uid);
    msg_tell(cur_sock_fd, string(msg));
}

void msg_up_not_exist(int from_uid)
{
    char msg[MSG_SIZE_MAX];
    sprintf(msg, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", from_uid, cur_uid);
    msg_tell(cur_sock_fd, string(msg));
}