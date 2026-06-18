#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
    char id[10];
    int burst_time; 
} DummyProcess;

void sort_queue_sjf(DummyProcess queue[], int size) {
    int i, j;
    DummyProcess temp;
    for (i = 0; i < size - 1; i++) {
        for (j = 0; j < size - i - 1; j++) {
            if (queue[j].burst_time > queue[j + 1].burst_time) {
                temp = queue[j];
                queue[j] = queue[j + 1];
                queue[j + 1] = temp;
            }
        }
    }
}

int main() {
    printf("========== RUNNING SJF UNIT TEST ==========\n");

    DummyProcess test_queue[3] = {
        {"P1", 15}, 
        {"P2", 3},  
        {"P3", 8}   
    };
    int size = 3;

    sort_queue_sjf(test_queue, size);

    assert(test_queue[0].burst_time == 3);
    assert(strcmp(test_queue[0].id, "P2") == 0);

    assert(test_queue[1].burst_time == 8);
    assert(strcmp(test_queue[1].id, "P3") == 0);

    assert(test_queue[2].burst_time == 15);
    assert(strcmp(test_queue[2].id, "P1") == 0);

    printf("SUCCESS: SJF Unit Test PASSED perfectly!\n");
    printf("===========================================\n");

    return 0;
}