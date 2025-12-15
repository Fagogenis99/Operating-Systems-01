// Main file
#include "message.h"

int main(int argc, char *argv[]){
    if (argc!=2){
        printf("Usage: %s <Dialog ID>\n", argv[0]);
        exit(1);
    }                                             //check for corect input
    int dialog_id = atoi(argv[1]);

    int shmid = shmget((key_t)SHM_KEY, sizeof(struct SharedMemory), 0666 | IPC_CREAT); // 0666 means everyone can read/write
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    return 0;
}
