/*

Will Simons
CS161
Assignment 3: Determine bounding boxes

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

#include "bounding_box.h" //contains struct for Bounding Box, bounding_box.c contains methods for manipulating.

/*
	Modify paths below to your opencv installation
*/
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cv.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cvaux.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/highgui.h"


#define DEBUG_BB 0
#define DEBUG_PRINT 0

#define READ_BUF_LENGTH 1024
#define MAX_DB_STATEMENT_BUFFER_LENGTH 5000

/*
	Change these variables to match your database.
*/
#define DBNAME "cs161"
#define HOST "localhost"
#define PORT "5432"
#define USER "postgres"
#define PASSWORD "password"


using namespace cv;
using namespace std;

int main(int argc, char *argv[]) {
	printf("HELLO WORLD\n");
}