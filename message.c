// Main file
#include "message.h"

int main(int argc, char *argv[]){
    if (argc!=2){
        printf("Usage: %s <Dialog ID>\n", argv[0]);
        exit(1);
    }                                             //check for corect input
    int dialog_id = atoi(argv[1]);
    
    // ask os for shared memory segment with key SHM_KEY
    int shmid = shmget((key_t)SHM_KEY, sizeof(struct SharedMemory), 0666 | IPC_CREAT); // 0666 means everyone can read/write
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    return 0;

    // attach to the shared memory segment
    void *shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }
    struct SharedMemory *shm=(struct SharedMemory *)shm_ptr;

    // create semaphore
    int semid = semget((key_t)SHM_KEY, 1, 0666 | IPC_CREAT);
    if(semid==-1){
        perror("semget failed");
        exit(1);
    }

    return 0;
}
