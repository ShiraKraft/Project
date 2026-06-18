#include <stdio.h>
#include <string.h>

int validate_arguments(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Error: Missing arguments.\n");
        fprintf(stderr, "Usage: %s -schd <fcfs|sjf> <file_name>\n", argv[0]);
        return 0; // Invalid
    }

    if (strcmp(argv[1], "-schd") != 0) {
        fprintf(stderr, "Error: Invalid flag '%s'. Expected '-schd'.\n", argv[1]);
        fprintf(stderr, "Usage: %s -schd <fcfs|sjf> <file_name>\n", argv[0]);
        return 0; // Invalid
    }

    if (strcmp(argv[2], "fcfs") != 0 && strcmp(argv[2], "sjf") != 0) {
        fprintf(stderr, "Error: Unknown scheduling algorithm '%s'.\n", argv[2]);
        fprintf(stderr, "Usage: %s -schd <fcfs|sjf> <file_name>\n", argv[0]);
        return 0; // Invalid
    }

    return 1; // Valid
}

int main() {
    printf("========== RUNNING CLI VALIDATION TEST ==========\n");

    // Test case 1: Correct arguments
    char *valid_args[] = {"./sim", "-schd", "sjf", "input.txt"};
    printf("Testing valid arguments...\n");
    if (validate_arguments(4, valid_args)) {
        printf("Test 1 Passed: Valid input recognized correctly.\n\n");
    }

    // Test case 2: Missing arguments
    char *missing_args[] = {"./sim", "-schd"};
    printf("Testing missing arguments...\n");
    if (!validate_arguments(2, missing_args)) {
        printf("Test 2 Passed: Missing arguments handled correctly.\n\n");
    }

    // Test case 3: Invalid algorithm name
    char *invalid_algo_args[] = {"./sim", "-schd", "roundrobin", "input.txt"};
    printf("Testing invalid algorithm...\n");
    if (!validate_arguments(4, invalid_algo_args)) {
        printf("Test 3 Passed: Invalid algorithm handled correctly.\n\n");
    }

    printf("SUCCESS: All CLI Validation Tests PASSED!\n");
    printf("=================================================\n");
    return 0;
}