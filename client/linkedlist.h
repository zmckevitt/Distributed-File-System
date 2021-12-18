#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define NUM_PAGES 4

struct Node {
	int pages[NUM_PAGES];
	char filename[128];
	struct Node* next;
};

struct Node* create_node(char* filename, int page);

struct Node* lookup(struct Node* head, char* filename);

void print_list(struct Node* head);

void delete_list(struct Node* head);

#endif