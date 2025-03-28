//
// Created by kiron on 3/26/25.
//

#include "stack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

Stack* createStack(int size, int canChange) {
    //Creates a new stack on the heap
    Stack* stack = malloc(sizeof(Stack));
    stack->capacity = size;
    stack->top = -1;
    stack->data = (int*)malloc(size * 4);
    stack->can_change_size = canChange;
    return stack;
}

void destroyStack(Stack* stack) {
    //Deallocates the stack from the heap
    free(stack->data);
    free(stack);
}

int push(Stack* stack, int keyAdded) {
    //Adds a new element to the top of the stack, resizes if required / moves data if not
    if (stack->top >= stack->capacity - 1) {
        if (stack->can_change_size == 1) {
            stack->data = (int*)realloc(stack->data, stack->capacity * 2 * 4);
            stack->capacity *= 2;
            if (!stack->data) {
                exit(EXIT_FAILURE);
            }
        } else {
            int* tmp = malloc(stack->capacity * 4);
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
    //Removes the top item on the stack
    if (stack->top == -1) {
        return 0;
    }
    *keyRecived = stack->data[stack->top];
    --stack->top;
    return 1;
}

int peek(Stack* stack) {
    //Returns the top item of the stack
    if (stack->top == -1) {
        return -1;
    }
    return stack->data[stack->top];
}

int clear(Stack* stack) {
    //Clears all elements on the stack
    if (stack->top == -1) {
        return 0;
    }
    free(stack->data);
    stack->top = -1;
    stack->data = malloc(stack->capacity * 4);
    return 1;
}