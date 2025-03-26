//
// Created by kiron on 3/26/25.
//

#include "stack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

Stack* createStack(int size) {
    Stack* stack = (Stack*)malloc(sizeof(Stack));
    stack->capacity = size;
    stack->top = -1;
    stack->data = (char**)malloc(size * sizeof(char*));
}

void destroyStack(Stack* stack) {
    int i;
    for (i = 0; i < stack->top + 1; i++) {
        free(stack->data[i]);
    }
    free(stack->data);
    free(stack);
}

int stackSize(Stack* stack) {
    return stack->top + 1;
}

int push(Stack* stack, char* str) {
    if (stack->top == stack->capacity - 1) {
        return 0;
    }
    stack->data[++stack->top] = malloc(strlen(str) + 1);
    memcpy(stack->data[stack->top], str, strlen(str) + 1);
    return 1;
}

int pop(Stack* stack, char* str) {
    if (stack->top == -1) {
        return 0;
    }
    memmove(str, stack->data[stack->top], strlen(stack->data[stack->top]));
    free(stack->data[stack->top]);
    --stack->top;
    return 1;
}

char* peek(Stack* stack) {
    return stack->data[stack->top];
}