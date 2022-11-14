#ifndef __USER_H
#define __USER_H

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>

using namespace std;

#define MAX_USER_ID 30

typedef struct user_
{
    int id;
    int pid;
    int port;

    // could not use string or char * because
    // one process could not get another process's read-only section
    char name[32];
    char ip[16];
    bool registered;
    int sem_id;
    int from_uid;

} user_t;

typedef struct shm_users_
{
    int shm_id;

    // 0:write_sem : one write at a time
    // 1:read_sem : one read at a time
    // 2:c1_sem : next write must after a read
    // 3:c2_sem : next read must after a write
    int sem_id;
    vector<int> unused_uid;
    // The users reside in shared memory, id=index
    user_t *users;
} shm_users_t;

void users_init();
void users_release();

int usr_add(struct sockaddr_in client_addr);
void usr_broadcast(char *msg, int type);
void usr_remove(int uid);
void usr_who();
void usr_update_name(string name);
void usr_tell(int uid, const char *msg);
void usr_yell(string msg_str);
bool usr_find_by_name(string name);
void usr_builtin_tell(int id, string msg);
void usr_pipe_open();
int usr_pipe_to(int to_uid, string cmdline);
int usr_pipe_from(int from_uid, string cmdline);
extern shm_users_t shm_users;
#endif