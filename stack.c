//
// Created by kiron on 3/26/25.
//

#include "stack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

Stack* createStack(int size, int canChange) {
    Stack* stack = malloc(sizeof(Stack));
    stack->capacity = size;
    stack->top = -1;
    stack->data = (int*)malloc(size * sizeof(int));
    stack->can_change_size = canChange;
    return stack;
}

void destroyStack(Stack* stack) {
    free(stack->data);
    free(stack);
}

int push(Stack* stack, int keyAdded) {
    if (stack->top >= stack->capacity - 1) {
        if (stack->can_change_size == 1) {
            stack->data = (int*)realloc(stack->data, stack->capacity * 2 * sizeof(int));
            stack->capacity *= 2;
            if (!stack->data) {
                exit(EXIT_FAILURE);
            }
        } else {
            int* tmp = malloc(stack->capacity * sizeof(int));
            memcpy(tmp, &stack->data[stack->capacity / 2 - 1], (stack->capacity * 2));
            stack->top = stack->capacity / 2 - 1;
            free(stack->data);
            stack->data = tmp;
        }
    }
    stack->data[++stack->top] = keyAdded;
    return 1;
}

int pop(Stack* stack, int* keyRecived) {
    if (stack->top == -1) {
        return 0;
    }
    *keyRecived = stack->data[stack->top];
    --stack->top;
    return 1;
}

int peek(Stack* stack) {
    if (stack->top == -1) {
        return -1;
    }
    return stack->data[stack->top];
}