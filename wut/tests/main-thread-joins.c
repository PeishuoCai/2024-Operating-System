#include "test.h"

#include "wut.h"

static int x = 0;

void run(void) {
    printf("run\n");
    shared_memory[2] = wut_id();
    x = 1;
}

void test(void) {
    wut_init();
    shared_memory[0] = wut_id();
    int id = wut_create(run);
    shared_memory[1] = id;
    shared_memory[3] = wut_join(id);
    shared_memory[4] = x;
}

void check(void) {
    expect(
        shared_memory[0], 0, "wut_id of the main thread is wrong"
    );
    expect(
        shared_memory[1], 1, "wut_id of the first thread is wrong"
    );
    expect(
        shared_memory[2], 1, "wut_id of the thread in run is wrong"
    );
    expect(
        shared_memory[3], 0, "wut_join on the first thread should return 0"
    );
    expect(
        shared_memory[4], 1, "the first thread should write to x"
    );
}
