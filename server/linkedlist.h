#include "../client/networking/networking.h"

#ifndef linkedlist
#define linkedlist
struct node
{
    char message[UNAMELENGTH+MESSAGELENGTH+6];
    struct node *next;
};

struct node * head;

void linkedListInit();
void addNode(char * msg);
char * getNode();
int nextNode();
int isNull();
int getLength();

#endif
