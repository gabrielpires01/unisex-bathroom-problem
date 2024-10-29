#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_CAPACITY 3
#define MAX_QUEUE_SIZE 100
#define INACTIVITY_TIMEOUT 2  // Time in seconds for inactivity to end program

// Mutex and condition variables
pthread_mutex_t bathroom_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty_bathroom = PTHREAD_COND_INITIALIZER;

// Struct to represent a person in the queue
typedef struct {
    int id;
    int gender;  // 1 for man, 2 for woman
} Person;

// Queue to track waiting individuals with their IDs and gender
Person waiting_queue[MAX_QUEUE_SIZE];
int queue_size = 0;
int queue_front = 0;

// Bathroom state
int men_in_bathroom = 0;
int women_in_bathroom = 0;
int next_id = 1;
time_t last_exit_time;

// Queue management functions
void enqueue(int id, int gender) {
    if (queue_size < MAX_QUEUE_SIZE) {
        waiting_queue[(queue_front + queue_size) % MAX_QUEUE_SIZE].id = id;
        waiting_queue[(queue_front + queue_size) % MAX_QUEUE_SIZE].gender = gender;
        queue_size++;
    }
}

Person dequeue() {
    Person p = {0, 0};  // Default person if queue is empty
    if (queue_size > 0) {
        p = waiting_queue[queue_front];
        queue_front = (queue_front + 1) % MAX_QUEUE_SIZE;
        queue_size--;
    }
    return p;
}

Person front_of_queue() {
    if (queue_size > 0) {
        return waiting_queue[queue_front];
    }
    return (Person){0, 0};  // Return a default person if queue is empty
}

void enter_bathroom(int id, int gender) {
    pthread_mutex_lock(&bathroom_lock);
    enqueue(id, gender);

    // Wait until conditions are met: bathroom has space, and the correct gender is at the front of the queue
    while ((gender == 1 && (women_in_bathroom > 0 || men_in_bathroom >= MAX_CAPACITY || front_of_queue().gender != 1)) ||
           (gender == 2 && (men_in_bathroom > 0 || women_in_bathroom >= MAX_CAPACITY || front_of_queue().gender != 2))) {
        pthread_cond_wait(&empty_bathroom, &bathroom_lock);
    }
    dequeue();  // Remove person from the queue as they are now entering

    // Update bathroom occupancy
    if (gender == 1) {
        men_in_bathroom++;
    } else {
        women_in_bathroom++;
    }

    pthread_mutex_unlock(&bathroom_lock);
}

void leave_bathroom(int gender) {
    pthread_mutex_lock(&bathroom_lock);

    // Update bathroom occupancy
    if (gender == 1) {
        men_in_bathroom--;
    } else {
        women_in_bathroom--;
    }

    // Update last exit time if bathroom is now empty
    if (men_in_bathroom == 0 && women_in_bathroom == 0 && queue_size == 0) {
        last_exit_time = time(NULL);
    }

    // If bathroom is empty for the current gender, allow next in line to proceed
    if ((gender == 1 && men_in_bathroom == 0) || (gender == 2 && women_in_bathroom == 0)) {
        pthread_cond_broadcast(&empty_bathroom);
    }

    pthread_mutex_unlock(&bathroom_lock);
}

// Thread function for men
void* man(void* arg) {
    int id = *((int*)arg);
    printf("Man %d wants to enter.\n", id);
    enter_bathroom(id, 1);
    printf("Man %d entered.\n", id);

    // Simulate bathroom usage
    sleep(rand() % 3 + 1);

    printf("Man %d is leaving.\n", id);
    leave_bathroom(1);
    return NULL;
}

// Thread function for women
void* woman(void* arg) {
    int id = *((int*)arg);
    printf("Woman %d wants to enter.\n", id);
    enter_bathroom(id, 2);
    printf("Woman %d entered.\n", id);

    // Simulate bathroom usage
    sleep(rand() % 3 + 1);

    printf("Woman %d is leaving.\n", id);
    leave_bathroom(2);
    return NULL;
}

void* arrival_generator(void* arg) {
    while (1) {
        int gender = rand() % 2 + 1;
        int id = next_id++;

        pthread_t thread;
        if (gender == 1) {
            pthread_create(&thread, NULL, man, &id);
        } else {
            pthread_create(&thread, NULL, woman, &id);
        }

        // Detach thread so it can run independently
        pthread_detach(thread);

        // Random delay for next arrival, sometimes exceeding INACTIVITY_TIMEOUT
        usleep((rand() % 2500 + 500) * 1000);  // 0.5 to 3 seconds

        // Check for inactivity
        pthread_mutex_lock(&bathroom_lock);
        if (difftime(time(NULL), last_exit_time) >= INACTIVITY_TIMEOUT && men_in_bathroom == 0 && women_in_bathroom == 0 && queue_size == 0) {
            pthread_mutex_unlock(&bathroom_lock);
            printf("No activity for %d seconds. Ending program.\n", INACTIVITY_TIMEOUT);
            exit(0);
        }
        pthread_mutex_unlock(&bathroom_lock);
    }
    return NULL;
}

int main() {
    srand(time(NULL));
    last_exit_time = time(NULL);

    pthread_t generator_thread;
    pthread_create(&generator_thread, NULL, arrival_generator, NULL);
    pthread_join(generator_thread, NULL);  // Wait for generator to finish (in this case, it will exit on inactivity)
    
    printf("All people have finished using the bathroom.\n");
    return 0;
}

