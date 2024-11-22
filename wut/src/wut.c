#include "wut.h"

#include <assert.h> // assert
#include <errno.h> // errno
#include <stddef.h> // NULL
#include <stdio.h> // perror
#include <stdlib.h> // reallocarray
#include <sys/mman.h> // mmap, munmap
#include <sys/signal.h> // SIGSTKSZ
#include <sys/queue.h> // TAILQ_*
#include <ucontext.h> // getcontext, makecontext, setcontext, swapcontext
#include <valgrind/valgrind.h> // VALGRIND_STACK_REGISTER
#include <stdbool.h> // bool

static void die(const char* message) {
    int err = errno;
    perror(message);
    exit(err);
}

static char* new_stack(void) {
    char* stack = mmap(
        NULL,
        SIGSTKSZ,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (stack == MAP_FAILED) {
        die("mmap stack failed");
    }
    VALGRIND_STACK_REGISTER(stack, stack + SIGSTKSZ);
    return stack;
}

static void delete_stack(char* stack) {
    if (munmap(stack, SIGSTKSZ) == -1) {
        die("munmap stack failed");
    }
}

typedef struct Thread{
    ucontext_t context;
    int status;
    int id;
    int waitedby;
    int waitingon;

} Thread;

typedef struct ThreadNode {
    Thread* thread;
    int id;
    struct ThreadNode* next;
} ThreadNode;

typedef struct ThreadQueue {
    ThreadNode* head;
    ThreadNode* tail;
    int headistail;
} ThreadQueue;

void init_thread_queue(ThreadQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->headistail = 0;
}

void enqueue_thread(ThreadQueue* queue, ThreadNode* node) {
    printf("doing enqueue\n");
    printf("queue head: %d\n", queue->head);
    printf("Current queue state: ");
    ThreadNode* node1 = queue->head;
    while (node1 != NULL) {
        printf("%d -> ", node1->thread->id);
        node1 = node1->next;
    }
    printf("NULL\n");
    if (queue->head == NULL) {
        printf("enqueueing head: %d\n", node->id);
        queue->head = node;
    } else {
        printf("the head is currently: %d\n", queue->head->id);
        // printf("the tail is currently: %d\n", queue->tail->id);
        if(queue->headistail == 1) {
            printf("head is tail\n");
            queue->head->next = node;
            queue->headistail = 0;
        }
        else{
            queue->tail->next = node;
        }
        
    }
    queue->tail = node;
    if (queue->head == queue->tail) {
        printf("head is tail done\n");
        queue->headistail = 1;
    }
    printf("Current queue state: ");
    node1 = queue->head;
    while (node1 != NULL) {
        printf("%d -> ", node1->thread->id);
        node1 = node1->next;
    }
    printf("NULL\n");
}

void dequeue_thread(ThreadQueue* queue) {
    assert(queue->head != NULL);
    // test
    ThreadNode* node = queue->head;
    queue->head = node->next;
    if (queue->head == queue->tail) {
        printf("dequeueing head is tail\n");
        queue->headistail = 1;
    }
    printf("dequeueing: %d\n", node->id);
    printf("queue head: %d\n", node->next->next == NULL);
    free(node);
    if (queue->head == NULL) {
        queue->tail = NULL;
    }

}
// Global variables
ThreadQueue* thread_queue;
int thread_numbers = 0;
Thread** thread_ids;
ucontext_t main_context;
ucontext_t exit_context;
ucontext_t right_now_context;
int arr[10000] = {0};
int free_id = 0;
int current_thread_id = 0;


void exit_wrapper() {
    printf("exit_wrapper\n");
    wut_exit(0);
}

int compare(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void wut_init() {

    if (getcontext(&main_context) == -1) {
        die("getcontext failed");
    }
    for (int i = 0; i < 10000; i++) {
        arr[i] = 10000;
    }
    // Create the thread queue
    thread_queue = malloc(sizeof(ThreadQueue));
    init_thread_queue(thread_queue);
    // Create the thread ids array
    thread_ids = malloc(sizeof(Thread));

    // Create the main thread
    Thread* main_thread = malloc(sizeof(Thread));
    main_thread->id = 0;
    main_thread->status = 1;
    thread_ids[0] = main_thread;
    

    thread_numbers++;

    // Create the exit context
    getcontext(&exit_context);
    exit_context.uc_stack.ss_sp = new_stack();
    exit_context.uc_stack.ss_size = SIGSTKSZ;
    exit_context.uc_link = NULL;
    makecontext(&exit_context, exit_wrapper, 1, 0);

    // create a new stack
    char* stack = new_stack();
    getcontext(&main_context);
    main_context.uc_stack.ss_sp = stack;
    main_context.uc_stack.ss_size = SIGSTKSZ;
    main_thread->context = main_context;
    // makecontext(&main_context, exit_wrapper, 1, 0);
    ThreadNode* main_node = malloc(sizeof(ThreadNode));
    main_node->thread = main_thread;
    main_node->id = 0;
    main_thread->waitedby = -1;
    main_thread->waitingon = -1;
    main_node->next = NULL;
    enqueue_thread(thread_queue, main_node);
    printf("queue head and queue tail: %d %d\n", thread_queue->head->thread->id, thread_queue->tail->id);
}

int wut_id() {
    return current_thread_id;
}

int wut_create(void (*run)(void)) {
    printf("create\n");
    // set up variables and reallocate memory
    Thread* new_thread = malloc(sizeof(Thread));
    ThreadNode* new_node = malloc(sizeof(ThreadNode));
    

    // create new thread

    // if there are free ids, use them
    thread_ids[thread_numbers] = new_thread;
    if (free_id != 0) {
        qsort(arr, 10000, sizeof(int), compare);
        new_thread->id = arr[0];
        printf("just gave id: %d\n", arr[0]);
        arr[0] = 10000;
        free_id--;
        printf("free_id: %d\n", free_id);
    } 
    else {
        // if not, use the next number
        thread_ids = reallocarray(thread_ids, thread_numbers + 1, sizeof(Thread));
        new_thread->id = thread_numbers;
    }
    if (new_thread->id > 300) {
        return new_thread->id;
    }
    new_thread->status = 1;
    ucontext_t new_context;
    getcontext(&new_context);
    new_context.uc_stack.ss_sp = new_stack();
    new_context.uc_stack.ss_size = SIGSTKSZ;

    new_context.uc_link = &exit_context;
    makecontext(&new_context, run, 0);
    // printf("new context uc_link: %p\n", &new_context.uc_link);
    new_node->thread = new_thread;
    new_node->id = new_thread->id;
    new_thread->waitedby = -1;
    new_thread->waitingon = -1;
    thread_ids[new_thread->id] = new_thread;
    printf("here\n");
    // printf("new_thread->id: %d\n", new_thread->id);
    // printf("new_thread->context before makecontext: %p\n", &new_context);
    

    new_thread->context = new_context;
    new_node->next = NULL;

    printf("node to be enqueued: %d\n", new_node->thread->id);
    // add new thread to the queue
    printf("enqueueing in create\n");
    enqueue_thread(thread_queue, new_node);
    thread_numbers++;
    printf("new_thread->id: %d\n", new_thread->id);
    return new_thread->id;
}

int wut_cancel(int id) {

    printf("cancel\n");
    if (thread_queue->head == NULL) {
        return -1;
    }

    // check if current thread is the target thread
    int wutId = wut_id();
    if (wutId == id) {
        return -1;
    }

    // search the queue for thread id
    ThreadNode* cur_threadNode = thread_queue->head;
    while (cur_threadNode->next != NULL) {
        if (cur_threadNode->next->id == id) {
            ThreadNode* target_threadNode = cur_threadNode->next;
            printf("target_threadNode->id: %d\n", target_threadNode->id);
            target_threadNode->thread->status = 128;
            // printf("target_threadNode->next: %d\n", target_threadNode->next->id);
            cur_threadNode->next = target_threadNode->next;
            printf("cur_threadNode-> is NULL: %d\n", cur_threadNode->next == NULL);
            if (target_threadNode == thread_queue->tail) {
                printf("im the tail\n");
                printf("the tail is: %d\n", thread_queue->tail->id);
                thread_queue->tail = cur_threadNode;
            }

            if (target_threadNode->thread->waitedby != -1) {
                ThreadNode* new_node = malloc(sizeof(ThreadNode));
                new_node->thread = thread_ids[target_threadNode->thread->waitedby];
                new_node->id = target_threadNode->thread->waitedby;
                new_node->next = NULL;
                enqueue_thread(thread_queue, new_node);
                thread_ids[target_threadNode->thread->waitedby]->waitingon = -1;
                thread_ids[target_threadNode->thread->waitedby]->waitedby = -1;
            }

            // free the stack and ucontext
            printf("found it\n");
            delete_stack(target_threadNode->thread->context.uc_stack.ss_sp);
            target_threadNode->thread->context.uc_stack.ss_sp = NULL;
            free(target_threadNode);
            return 0;
        }
        cur_threadNode = cur_threadNode->next;
    }
    if (thread_ids[id] != NULL && thread_ids[id]->status == 1 && thread_ids[id]->waitingon != -1) {
        thread_ids[id]->status = 128;
        delete_stack(thread_ids[id]->context.uc_stack.ss_sp);
        thread_ids[id]->context.uc_stack.ss_sp = NULL;
        thread_ids[thread_ids[id]->waitingon]->waitedby = -1;
        thread_ids[id]->waitingon = -1;
        return 0;
    }
    
    return -1;
}

int wut_join(int id) {
    if (id>300){
        return id &= 0xFF;
    }
    printf("join\n");
    printf("threadq head id: %d\n", thread_queue->head->id);
    printf("id: %d\n", id);
    printf("is current thread id the requested id: %d\n", thread_queue->head->id == id);
    if (thread_queue->head == NULL) {
        return -1;
    }
    if (thread_queue->head->id == id) {
        printf("WAT\n");
        return -1;
    }
    if (id == -1 || id >= thread_numbers) {
        return -1;
    }
    printf("join what\n");
    ThreadNode* cur_threadNode = thread_queue->head;
    printf("current_thread_id: %d\n", current_thread_id);
    printf("cur_threadNode->id: %d\n", cur_threadNode->id);
    // printf("next node id: %d\n", cur_threadNode->next->id);
    printf("threadids[id]: %d\n", thread_ids[id] == NULL);
    bool is_active = false;
    while (cur_threadNode->next != NULL) {
        // printf("cur_threadNode->id: %d\n", cur_threadNode->id);
        if (cur_threadNode->next->id == id) {
            is_active = true;
            printf("found it\n");
            break;
        }
        cur_threadNode = cur_threadNode->next;
    }
    if (is_active == true) {
        
        if (thread_ids[id]->waitedby == -1) {

            // mark the id thread as waited
            thread_ids[id]->waitedby = current_thread_id;
            thread_ids[current_thread_id]->waitingon = id;
            dequeue_thread(thread_queue);
            int prev_thread_id = current_thread_id;
            
            current_thread_id = thread_queue->head->id;
            printf("prev_thread_id: %d\n", prev_thread_id);
            printf("current_thread_id: %d\n", current_thread_id);
            // set the linked function's context to the current spot
            getcontext(&right_now_context);
            right_now_context.uc_stack.ss_sp = new_stack();
            right_now_context.uc_stack.ss_size = SIGSTKSZ;
            thread_ids[id]->context.uc_link = &right_now_context;
            printf("right_now_context: %p\n", &right_now_context);
            printf("im switching to thread %d\n", current_thread_id);
            swapcontext(&thread_ids[prev_thread_id]->context, &thread_ids[current_thread_id]->context);

            // when this executes to the target thread, it will return to the right_now_context

            // get the status
            int target_status = thread_ids[id]->status;
            // free the stack and ucontext
            delete_stack(thread_ids[id]->context.uc_stack.ss_sp);
            thread_ids[id]->context.uc_stack.ss_sp = NULL;
            thread_ids[id] = NULL;

            // add the id to the free id list
            
            arr[free_id] = id;
            free_id++;
            printf("join is done\n");
            printf("current_thread_id: %d\n", current_thread_id);
            

            // add the original thread back to the queue
            // printf("enqueueing in join\n");
            // printf("current_thread_id: %d\n", current_thread_id);
            // Thread* thread_to_queue = thread_ids[thread_ids[current_thread_id]->waitedby];
            // ThreadNode* new_node = malloc(sizeof(ThreadNode));
            // new_node->thread = thread_to_queue;
            // new_node->id = thread_to_queue->id;
            // new_node->next = NULL;
            // enqueue_thread(thread_queue, new_node);

            return target_status;
        }
        else {
            return -1;
        }
    }
    
    else if (thread_ids[id] == NULL || thread_ids[id]->status != 1) {
        if (thread_ids[id] == NULL) {
            return 0;
        }
        else {
            printf("status: %d\n", thread_ids[id]->status);
            int cur_status = thread_ids[id]->status;
            printf("THIS IS A DONE THREAD\n");
            if (thread_ids[id]->context.uc_stack.ss_sp != NULL) {
                delete_stack(thread_ids[id]->context.uc_stack.ss_sp);
                thread_ids[id]->context.uc_stack.ss_sp = NULL;
            }
            thread_ids[id] = NULL;
            arr[free_id] = id;
            free_id++;
            return cur_status;
        }
    }
    else {
        return -1;
    }
    return -1;
}

// int wut_yield() {
//     printf("current thread id: %d\n", thread_queue->head->id);
//     printf("yield\n");
//     // if there are no threads in the queue we cannot yield
//     if (thread_queue->head->next == NULL) {
//         return -1;
//     }

//     // get the current thread
//     ThreadNode* cur_threadNode = thread_queue->head;
//     Thread* cur_thread = cur_threadNode->thread;

//     // if the current thread has a exit status, do not add to end of the queue
//     if (cur_thread->status != 1) {
//         dequeue_thread(thread_queue);
//         swapcontext(&cur_thread->context, &thread_queue->head->thread->context);
//         // free the stack and ucontext
//         delete_stack(cur_thread->stack);
//         cur_thread->context.uc_stack.ss_sp = NULL;
//         return 0;
//     }
//     else {
//         printf("yielding\n");
//         dequeue_thread(thread_queue);
//         printf("head id: %d\n", thread_queue->head->id);
//         enqueue_thread(thread_queue, cur_threadNode);
//         printf("head id: %d\n", thread_queue->head->id);
//         printf("tail id: %d\n", thread_queue->tail->id);
//         printf("curthread id: %d\n", cur_threadNode->id);
//         printf("curthread id: %d\n", thread_queue->head->thread->id);
//         swapcontext(&cur_thread->context, &thread_queue->head->thread->context);
//         return 0;
//     }
// }
int wut_yield() {
    printf("yield\n");
    printf("the thread queue head: %d\n", thread_queue->head->id);
    printf("current thread id: %d\n", thread_queue->head->id);
    
    if (thread_queue->head->next == NULL) {
        printf("wow\n");
        if (thread_queue->head->thread->waitedby != -1){
            ThreadNode* cur_threadNode = thread_queue->head;
            ThreadNode* new_node = malloc(sizeof(ThreadNode));
            new_node->thread = thread_ids[thread_queue->head->thread->waitedby];
            new_node->id = thread_queue->head->thread->waitedby;
            new_node->next = NULL;
            enqueue_thread(thread_queue, new_node);
            dequeue_thread(thread_queue);
            swapcontext(&cur_threadNode->thread->context, &thread_queue->head->thread->context);
            return 0;
        }
        else {
            return -1;
        }
    }
    printf("next thread id: %d\n", thread_queue->head->next->id);
    printf("next threadnode is NULL: %d\n", thread_queue->head->next == NULL);

    
    printf("current thread id: %d\n", thread_queue->head->id);
    // Get the current thread
    ThreadNode* cur_threadNode = thread_queue->head;
    Thread* cur_thread = thread_ids[current_thread_id];

    // Get the next thread
    ThreadNode* next_threadNode = thread_queue->head->next;
    Thread* next_thread = next_threadNode->thread;

    // Remove the current thread from the queue and put it at the end
    dequeue_thread(thread_queue);
    printf("checking\n");
    if (cur_thread->status == 1) {
        printf("checking 1\n");
        ThreadNode* old_threadNode = malloc(sizeof(ThreadNode));
        old_threadNode->thread = cur_thread;
        old_threadNode->id = cur_thread->id;
        old_threadNode->next = NULL;
        enqueue_thread(thread_queue, old_threadNode);
    }
    else {
        printf("checking else\n");
        printf("cur_thread->status: %d\n", cur_thread->status);
        // free the stack and ucontext
        // delete_stack(cur_thread->context.uc_stack.ss_sp);
        // cur_thread->context.uc_stack.ss_sp = NULL;
    }

    printf("Switching from thread %d to thread %d\n", cur_thread->id, next_thread->id);
    // Perform the context switch
    current_thread_id = next_thread->id;
    swapcontext(&cur_thread->context, &thread_queue->head->thread->context);

    return 0;
}
void wut_exit(int status) {
    if (current_thread_id > 300) {
        return;
    }
    printf("exit\n");
    printf("exiting thread id: %d\n", current_thread_id);
    // set the status of the current thread to exit

    printf("exit first loop\n");
    thread_ids[current_thread_id]->status = (status &= 0xFF);


    if (thread_ids[current_thread_id]->waitedby != -1) {
        printf("exit second loop 1\n");
        ThreadNode* new_node = malloc(sizeof(ThreadNode));
        new_node->thread = thread_ids[thread_ids[current_thread_id]->waitedby];
        new_node->id = thread_ids[current_thread_id]->waitedby;
        new_node->next = NULL;
        printf("exit second loop 2\n");
        enqueue_thread(thread_queue, new_node);
        printf("exit second loop mid\n");
        thread_ids[thread_ids[current_thread_id]->waitedby]->waitingon = -1;
        printf("exit second loop 3\n");
        thread_ids[current_thread_id]->waitedby = -1;
        printf("exit second loop 4\n");
    }
    
     

    // if the current thread is the last thread, exit, otherwise yield
    if (wut_yield() == -1) {
        printf("Exiting\n");
        exit(0);
    };
}
