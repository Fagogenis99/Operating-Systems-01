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

    struct shmid_ds shm_info;
    shmctl(shmid, IPC_STAT, &shm_info);  // get shared memory info

    if(shm_info.shm_nattch==1){  // if this is the first process to attach
        // initialize shared memory
        union semun arg;
        arg.val = 1;  // binary semaphore initialized to 1
        if(semctl(semid, 0, SETVAL, arg)==-1){
            perror("semctl failed");
            exit(1);
        }
        // clean everything
        shm->latest_message_id = 0;
        shm->total_users = 0; 
        for(int i=0; i<MAX_DIALOGS; i++){
            shm->dialogs[i].id = 0;   // mark all dialog slots as free
            shm->dialogs[i].user_count = 0;
        }
        for(int i=0;i<MAX_MSGS;i++){
            shm->msgs[i].is_free = 1; // mark all message slots as free
            shm->msgs[i].readers_left = 0;
        }
    }
    if (1){  // always true 
        struct sembuf lock = {0, -1, 0};   // Wait/Lock (-1)
        struct sembuf unlock = {0, 1, 0};  // Signal/Unlock (+1)

        // lock semaphore before accessing shared memory
        if(semop(semid, &lock, 1)==-1){
            perror("semop lock failed");
            exit(1);
        }

        // check if dialog exists
        int dialog_index = -1;
        for(int i=0; i<MAX_DIALOGS; i++){
            if(shm->dialogs[i].id == dialog_id){ // check if dialog exists
                dialog_index = i;
                break;
            }
        }

        if (dialog_index==-1){ // if dialog does not exist, create it
            for (int i=0; i<MAX_DIALOGS; i++){
                if (shm->dialogs[i].id==0){ // find free slot
                    shm->dialogs[i].id = dialog_id;
                    // shm->dialogs[i].user_count = 1;
                    dialog_index = i;
                    printf("Created new dialog with ID %d\n", dialog_id);
                    break;
                }
            }
        }
        if (dialog_index==-1){
            printf("No free dialog slots available\n");
            if(semop(semid, &unlock, 1)==-1){
                perror("semop unlock failed");
                exit(1);
            }
            exit(1);
        }
        shm->dialogs[dialog_index].user_count++; //update user counts
        shm->total_users++;
        printf("Joined dialog %d. Users in dialog: %d. Total users: %d\n", dialog_id, shm->dialogs[dialog_index].user_count, shm->total_users);
        
        int next_expected_id = shm->latest_message_id + 1; // next message id to read
        // unlock semaphore
        if(semop(semid, &unlock, 1)==-1){
            perror("semop unlock failed");
            exit(1);
        }

        //fork 
        pid_t pid=fork();
        if (pid<0){
            perror("fork failed");
            exit(1);
        }
        if (pid==0){ //  child | reader
            while(1){
                if (semop(semid, &lock, 1)==-1){ // lock semaphore
                    perror("semop lock failed");
                    exit(1);
                }

                int index=-1;
                for (int i=0; i<MAX_MSGS; i++){ // find message with next_expected_id in the dialog
                    if ((shm->msgs[i].id == next_expected_id) && (shm->msgs[i].dialog_id == dialog_id) && (shm->msgs[i].is_free == 0)){
                        index=i;
                        break;
                    }
                }
                if (index!=-1){ // if found
                    if (shm->msgs[index].sender_pid != getppid()) {
                        printf("Dialog %d: %s\n", dialog_id, shm->msgs[index].text);
                    }

                    shm->msgs[index].readers_left--;
                    if (strcmp(shm->msgs[index].text, "TERMINATE") == 0) {
                        if (shm->msgs[index].readers_left <= 0){
                            shm->msgs[index].is_free = 1; // mark as free
                        }
                        shm->dialogs[dialog_index].user_count--;
                        if (shm->dialogs[dialog_index].user_count <= 0) { // if no users left, close dialog
                            shm->dialogs[dialog_index].id = 0; 
                            shm->dialogs[dialog_index].user_count = 0;
                            printf("Dialog %d is now empty and terminated.\n", dialog_id);
                        }
                        semop(semid, &unlock, 1);            // unlock before exiting
                        if(shm->msgs[index].sender_pid!=getppid()){
                            printf("Exiting dialog %d.\n", dialog_id);
                            kill(getppid(), SIGTERM); // terminate parent process
                        }
                        exit(0);
                    }
                    next_expected_id++;

                    if (shm->msgs[index].readers_left <= 0){
                        shm->msgs[index].is_free = 1; // mark as free
                    }
                    semop(semid, &unlock, 1);            // unlock semaphore
                }else{
                    semop(semid, &unlock, 1);            // unlock semaphore
                    usleep(100000);                      // sleep before checking again
                }
            }
        }else{ //  parent | writer
            char input[TEXT_SIZE];
            while(1){
                if (fgets(input, TEXT_SIZE, stdin) == NULL) {
                    printf("\nError reading input. Exiting.\n");
                    strcpy(input, "TERMINATE");
                }else{
                    input[strcspn(input, "\n")] = 0;    // remove newline
                }
                if(input[0]=='\0'){
                    continue;                           // ignore empty messages
                }
                if(semop(semid, &lock, 1)==-1){         // lock semaphore
                    perror("semop lock failed");
                    exit(1);
                }
                int free_slot=-1;
                for(int i=0; i<MAX_MSGS; i++){          // find free message slot
                    if(shm->msgs[i].is_free==1){
                        free_slot=i;
                        break;
                    }
                }

                if (free_slot==-1){
                    printf("No free message slots available. Message not sent.\n");
                    semop(semid, &unlock, 1);            // unlock semaphore
                    sleep(1);
                    continue;
                }
                // write message to shared memory
                shm->latest_message_id++;
                shm->msgs[free_slot].id = shm->latest_message_id;
                shm->msgs[free_slot].dialog_id = dialog_id;
                shm->msgs[free_slot].sender_pid = getpid();
                strcpy(shm->msgs[free_slot].text, input);
                shm->msgs[free_slot].is_free = 0;

                int readers=shm->dialogs[dialog_index].user_count; 
                shm->msgs[free_slot].readers_left = (readers > 0) ? readers : 0;

                if ((shm->msgs[free_slot].readers_left == 0) && (strcmp(input, "TERMINATE") != 0)) {
                    shm->msgs[free_slot].is_free = 1; // mark as free if no readers
                }
                
                semop(semid, &unlock, 1);            // unlock semaphore
                if (strcmp(input, "TERMINATE") == 0) {
                    wait(NULL);                      // wait for child to finish
                    printf("Exiting dialog %d.\n", dialog_id);
                    break;
                }
            }
        }
    
    }
    return 0;
}