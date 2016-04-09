#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>

#include "bounding_box.h"
#include "linkedlist.h"

/*
	Modify paths below to your opencv installation
*/
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cv.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cvaux.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/highgui.h"

/*
	Change these variables to match your database.
*/
#define DBNAME "cs161"
#define HOST "localhost"
#define PORT "5432"
#define USER "postgres"
#define PASSWORD "password"

#define DEBUG_IMAGE 0
#define DEBUG_PRINT 0

#define READ_BUF_LENGTH 1024
#define MAX_DB_STATEMENT_BUFFER_LENGTH 5000

static List *list = NULL;

struct frame_args {
	int video_id;
	int frame_number;
	char *temp_directory;
};

void List_print(List *list) {
	struct frame_args *p_fas;
	LIST_FOREACH(list, head, next, cur) {
		if(cur->prev == NULL) printf("Head: ");
		else if(cur->next == NULL) printf("Tail: ");
		else printf("Node: ");
		p_fas = (frame_args *)cur->value;
		printf("%s %d %d\n", p_fas->temp_directory, p_fas->video_id, p_fas->frame_number);
	}
}

void List_clear_frame_args(List *list) {
	LIST_FOREACH(list, head, next, cur) {
		if(cur->prev == NULL) printf("Head: ");
		else if(cur->next == NULL) printf("Tail: ");
		else printf("Node: ");
		p_fas = (frame_args *)cur->value;
		printf("%s %d %d\n", p_fas->temp_directory, p_fas->video_id, p_fas->frame_number);
	}
}

int main(int argc, char *argv[])
{
	list = List_create();
	int i, list_size = 10;
	struct frame_args *fas;
	

	fas = (frame_args *) malloc(sizeof(frame_args) * list_size);

	for (i = 0; i < list_size; i++) {
		struct frame_args fa = {
			.video_id = 2,
			.frame_number = i + 1,
			.temp_directory = "hello"
		};
		fas[i] = fa;

		List_push(list, &fas[i]);
	}
	

	List_print(list);

	// List_clear(list);
	free(fas);

	List_print(list);


	return 0;
}