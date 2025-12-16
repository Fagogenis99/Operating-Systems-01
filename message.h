#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>


#define SHM_KEY 2100199     // shared memory key
#define MAX_MSGS 200        // max number of messages
#define TEXT_SIZE 100       // max size of message text
#define MAX_DIALOGS 10      // max number of dialog rooms

struct Message {
    int id;                 // message id
    int dialog_id;          // which dialog room this belongs to
    pid_t sender_pid;       // who sent this message
    char text[TEXT_SIZE];   // message text
    int readers_left;       // how many should read read this before deletion
    int is_free;            // 1 if empty slot, 0 if occupied
};

struct Dialog {
    int id;                 // dialog room id
    int user_count;         // users in the room
    int is_free;            // 1 if free, 0 if occupied
};

struct SharedMemory {
    int latest_message_id;               // id of the latest message
    struct Dialog dialogs[MAX_DIALOGS];  // all dialogs
    struct Message msgs[MAX_MSGS];       // all messages
    int total_users;                     // total users in the system
};

union semun {
    int val;                // OPTION A: Set a specific integer value
    struct semid_ds *buf;   // OPTION B: specific permissions/stats
    unsigned short *array;  // OPTION C: Set multiple values at once
};

#endif // MESSAGE_H