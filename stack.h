//
// Created by kiron on 3/26/25.
//

#ifndef STACK_H
#define STACK_H

typedef struct Stack {
    char** data;
    int top, capacity;
} Stack;

Stack* createStack(int size);
void destroyStack(Stack* stack);
int stackSize(Stack* stack);
int push(Stack* stack, char* str);
int pop(Stack* stack, char* str);
char* peek(Stack* stack);


#endif //STACK_H
