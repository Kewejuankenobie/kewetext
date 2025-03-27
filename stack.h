//
// Created by kiron on 3/26/25.
//

#ifndef STACK_H
#define STACK_H

//Integer Stack Data Structure used to store the integer values of key presses

typedef struct Stack {
    int* data;
    int top, capacity, can_change_size;
} Stack;

Stack* createStack(int size, int canChange);
void destroyStack(Stack* stack);
int push(Stack* stack, int keyAdded);
int pop(Stack* stack, int* keyRecived);
int peek(Stack* stack);
int clear(Stack* stack);


#endif //STACK_H
