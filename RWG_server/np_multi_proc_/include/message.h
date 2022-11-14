#ifndef __MESSAGE_H
#define __MESSAGE_H

#include <iostream>
#include <string>
#define MSG_SIZE_MAX 1024

// Message type
#define MSG_NONE 0
#define MSG_LOGOUT 1
#define MSG_TELL 2

// undefined variable
#define MSG_CANT_EXCEED 0xcc
#define MSG_STR_DELIM 0xcd

typedef struct message_str
{
    char tag;
    char content[0];
} message_str_t;


typedef struct message_
{
    int write_offset;

    // ring buffer
    char mem[MSG_SIZE_MAX];
} message_t;

typedef struct shm_msg_
{
    // reside in shared memory
    message_t *msg;

    int shm_id;

    // 0:write_sem : one write at a time
    // 1:read_sem : one read at a time
    int sem_id;
    int read_offset;

} shm_msg_t;

void msg_init();
void msg_welcome(int sock_fd);
void msg_tell(int sock_fd, std::string msg);
void msg_prompt(int client_sock);
void msg_read_msg();
void msg_write_msg(const char *content, int type, int arg);
void msg_release();
void msg_who();
void msg_user_not_exist(int uid);
void msg_up_exist(int to_uid);
void msg_user_not_exist(int uid);
void msg_up_not_exist(int from_uid);

extern shm_msg_t shm_msg;

#endif