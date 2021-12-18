#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "linkedlist.h"

struct Node* create_node(char* filename, int page) {

	struct Node* node = malloc(sizeof(struct Node));
	strcpy(node->filename, filename);

	// initialize 
	for(int i=0; i<NUM_PAGES; ++i) {
		node->pages[i] = 0;
	}

	node->pages[page-1] = 1;
	node->next = NULL;

	return node;
}

struct Node* lookup(struct Node* head, char* filename) {

	struct Node* curr = head;

	while(curr != NULL) {

		// matching filenames
		if(strcmp(curr->filename, filename) == 0) {
			return curr;
		}

		curr=curr->next;
	}

	return NULL;
}

void print_list(struct Node* head) {

	struct Node* curr = head;
	while(curr != NULL) {

		printf("%s", curr->filename);
		for(int i=0; i<NUM_PAGES; ++i) {
			if(curr->pages[i] == 0) {
				printf(" [incomplete]");
				break;
			}
		}
		printf("\n");

		curr=curr->next;
	}
}

void delete_list(struct Node* head) {

	struct Node* curr = head;
	struct Node* next;

	while(curr != NULL) {
		// printf("FREEING: %s\n", curr->filename);
		next = curr->next;
		free(curr);
		curr = next;
	}

}
