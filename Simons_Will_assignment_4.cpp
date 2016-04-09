/*

Will Simons
CS161 - Bruce
Assignment 4: Draw Bounding Boxes

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#include "bounding_box.h" //contains struct for Bounding Box, bounding_box.c contains methods for manipulating.

/*
	Modify paths below to your opencv installation
*/
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cv.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cvaux.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/highgui.h"


#define DEBUG_IMAGE 0
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

#ifndef MAX_DB_CONNECTIONS
#define MAX_DB_CONNECTIONS 4
#endif

void *process_frame(void *frame_arg);
int get_video_meta(int video_id, int *num_frames, int *frame_width, int *frame_height, float *fps, PGconn *pgc);
int does_dir_exist(char *dir_name);
char *create_temp_directory();
void remove_temp_directory(char *temp_directory);
void save_temp_image(Mat source_image, int video_id, int frame_number, char *temp_directory);
Mat draw_bounding_boxes(char *input_filename, int video_id, int frame_number, PGconn *pgc);
BoundingBox get_bounding_box(char const *bbox_type, int video_id, int frame_number, PGconn *pgc);
void draw_rectangle(Mat *source_image, BoundingBox bb, Scalar clr);
void make_video(float fps, int video_id, char *dir);

PGconn *database_connect(); //Returns new connection to database
void database_disconnect(); //Disconnects all databse connections.
int get_db_connection(PGconn **pgc); //Returns index of connection to databse.
void release_db_connection(int pgc_index); //Releases a connection to be used by pthread.

//Struct to be passed as argument to pthreads
struct frame_args {
	int video_id;
	int frame_number;
	char *temp_directory;
};

int totalFinished = 0; //Keeps track of how many frames have finished.
PGconn *connections[MAX_DB_CONNECTIONS]; //The actual database connections.
int openConnections[MAX_DB_CONNECTIONS]; //Keep track of which connections are open. (0 is available, 1 is in use.)
PGconn *conn; //Database connection to keep open.
sem_t *sem_db_conn; //Named semaphore for locking access to db connections.


int main(int argc, char *argv[]) {

	char input_filename[1280];
	char *temp_directory;
	int video_id, num_frames, frame_width, frame_height, i, ret;
	float fps;
	Mat source_image;
	//Threading declarations.
	pthread_attr_t attr;
	int result_code, index;

	if (argc < 2) {
		printf("Program accepts 2 args. ./%s <video_id>\n", argv[0]);
		exit(1);
	}

	video_id = atoi(argv[1]);


	if(video_id < 0) {
		printf("Please enter a positive video_id number.\n");
		exit(1);
	}

	//Create database connections.
	for (int i = 0; i < MAX_DB_CONNECTIONS; i++) {
		connections[i] = database_connect();
	}



	//retrieve video meta data from the database.
	PGconn *_pgc;
	int _index = get_db_connection(&_pgc);
	ret = get_video_meta(video_id, &num_frames, &frame_width, &frame_height, &fps, _pgc);
	if(!ret) {
		printf("Video with id %d does not exist in database.\n", video_id);
		database_disconnect();
		exit(1);
	}
	release_db_connection(_index);

	//Check that we have extracted images for the video to our image direcory.
	char *img_dir = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);
	snprintf(img_dir, READ_BUF_LENGTH, "images/%d", video_id);
	ret = does_dir_exist(img_dir);
	if(!ret) {
		printf("No directory at %s containing images exists. Make sure you extract images first (Assignment 2).\n", img_dir);
		database_disconnect();
		free(img_dir);
		exit(1);
	} else {
		printf("%s", img_dir);
	}
	free(img_dir);

	

	//Create a temporary directory to hold the drawn on images.
	temp_directory = create_temp_directory();

	//print out some info on video
	printf("Using %d db connections.\n", MAX_DB_CONNECTIONS);
	printf("Processing video %d with %d frames, width %d, height %d, at %.2f fps.\n", video_id, num_frames, frame_width, frame_height, fps);

	//Initialize arrays to track threads and thread arguments.
	pthread_t threads[num_frames];
	struct frame_args thread_args[num_frames];
	//Initialize pthread_attr
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	//unlink named semaphore in case already exists.
	sem_unlink("sem_db_conn");
	sem_db_conn = sem_open("sem_db_conn", O_CREAT|O_EXCL, 0644, MAX_DB_CONNECTIONS);

	//If we could not open semaphore, exit.
	if(sem_db_conn == SEM_FAILED) {
		printf("Init semaphore failed\n");
		database_disconnect();
		exit(0);
	}

	/* 
	Meat and potatoes section.
	Create threads that find BoundingBoxes, draw them, save temp images.
	*/
	//create threads
	for(index = 0; index < num_frames; index++) {		
		frame_args fa = {.video_id = video_id, .frame_number = index + 1, temp_directory = temp_directory};
		thread_args[index] = fa;
		result_code = pthread_create(&threads[index], &attr, process_frame, &thread_args[index]);
		assert(0 == result_code);
	}
	//join threads
	for (index = 0; index < num_frames; ++index) {
    // block until thread 'index' completes
    result_code = pthread_join(threads[index], NULL);
    totalFinished++;
    printf("\rTotal Finished %d/%d", totalFinished, num_frames);
    fflush(stdout);
    assert(0 == result_code);
  }


  //close and unlink named semaphore
  sem_close(sem_db_conn);
  sem_unlink("sem_db_conn");

  //Close database connections.
  database_disconnect();

	//Make video from drawn-on images in temp_directory.
	make_video(fps, video_id, temp_directory);
	//Delete temp_directory
	remove_temp_directory(temp_directory);

	printf("\n\tFinished Processing\n");
	exit(0);
}

/*

Passed as argument to pthreads.
Waits on semaphore for available database connection.
Enters critical section, calls draw_bounding_boxes 
which queries the database for all bounding boxes for this frame.
Then, draws on image and saves it.

*/
void *process_frame(void *frame_arg) 
{
	char input_filename[READ_BUF_LENGTH];
	Mat source_image;
	frame_args fa;

	fa = *((frame_args *) frame_arg);
	snprintf(input_filename, READ_BUF_LENGTH, "images/%d/0.%d.png", fa.video_id, fa.frame_number);

	PGconn *pgc;
	int db_connection_index;
	int lock_status;

	//Begin critical section
	lock_status = sem_wait(sem_db_conn); //wait for db_connection

	db_connection_index = get_db_connection(&pgc); //get db_connection
	source_image = draw_bounding_boxes(input_filename, fa.video_id, fa.frame_number, pgc);
	release_db_connection(db_connection_index); //release db_connection

	sem_post(sem_db_conn); //Increase semaphore value.
	//End critical section
	
	//If there is a source_image, save it!
	if(!source_image.empty()) {
		save_temp_image(source_image, fa.video_id, fa.frame_number, fa.temp_directory);
	}

	//done!
	pthread_exit(NULL);
}

/*
	Find unused connection.  Iterates through openConnections array and looks for
	value of 0 (open).  Returns position in openConnections/connections of PGconn,
	and points the parameter at the open connection in the connections array.
*/
int get_db_connection(PGconn **pgc) {
	int i;
	for(i = 0; i < MAX_DB_CONNECTIONS; i++) {
		if(openConnections[i] == 0) {
			*pgc = connections[i];
			openConnections[i] = 1;
			return i;
		}
	}
	//No open connections
	return -1;
}

/*
	Releases database connection. Sets openConnections[index] to 0, signifying the connections is available for use.
*/
void release_db_connection(int pgc_index) {
	openConnections[pgc_index] = 0;
}

void database_disconnect()
{
	for (int i = 0; i < MAX_DB_CONNECTIONS; i++) {
		PQfinish(connections[i]);
	}
}

/*
	Returns a new connection to the database.
*/
PGconn *database_connect()
{
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];

	snprintf(&conninfo[0], MAX_DB_STATEMENT_BUFFER_LENGTH,"dbname = '%s' host = '%s' port = '%s' user = '%s' password = '%s'",
				DBNAME, HOST, PORT, USER, PASSWORD);
	PGconn *pgc = PQconnectdb(conninfo);
	if(PQstatus(pgc) != CONNECTION_OK) 
	{
		printf("Connection to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(0);
	} else {
		return pgc;
	}
}


/*

	Gathers video meta info.
	Points num_frames, frame_width, frame_height, fps at correct values.
	Returns 0 if failed,
					1 if successful.
*/
int get_video_meta(int video_id, int *num_frames, int *frame_width, int *frame_height, float *fps, PGconn *pgc)
{
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];
	PGresult *res;
	int num_rows;
	int i, j;

	//check that connection is still alive
	if(PQstatus(pgc) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %d: %s\n", PQstatus(pgc), PQerrorMessage(pgc));
		PQfinish(pgc);
		exit(EXIT_FAILURE);
	}

	snprintf(&db_statement[0], MAX_DB_STATEMENT_BUFFER_LENGTH, "select num_frames, x_resolution, y_resolution, fps from video_meta where video_id=%d;",
					video_id);

	res = PQexec(pgc, &db_statement[0]);

	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			printf("Executed command, no return.\n");
			return 0;
			break;

		case PGRES_TUPLES_OK: 

			num_rows = PQntuples(res);

			if(num_rows == 0) {
				return 0;
			}

			*num_frames = atoi(PQgetvalue(res, 0, 0));
			*frame_width = atoi(PQgetvalue(res, 0, 1));
			*frame_height = atoi(PQgetvalue(res, 0, 2));
			*fps = atof(PQgetvalue(res, 0, 3));
			
			break;

		default:
			printf("Something went wrong.\n");
			return 0;
	}

	PQclear(res);
	return 1;
}



/*
	Returns 1 if directory exists,
					0 if directory otherwise.
*/
int does_dir_exist(char *dir_name)
{
	struct stat st = {0};
	if(stat(dir_name, &st) == 0) {
		return 1;
	} else {
		return 0;
	}
}

/*
	Creates temporary directory.
	Returns name of directory.
*/
char *create_temp_directory() {
	char *dir = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);
	sprintf(dir, "temp");

	struct stat st = {0};
	if(stat(dir, &st) == -1) {
		mkdir(dir, 0700);
		printf("\tCreated temp directory: %s", dir);
	}

	printf("\t%s", dir);
	return dir;
}

/*
	Remove directory with path dir_name.
*/
void remove_temp_directory(char *dir_name) {
	FILE *fp;
	char *command = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);

	sprintf(command, "rm -r ");
	strlcat(command, dir_name, READ_BUF_LENGTH);

	fp = popen(command, "r");
	if( fp == NULL ) {
		printf("\n\tCouldn't remove directory %s\n", dir_name);
	} else {
		printf("\tRemoved Temp Directory.\n");
	}

	pclose(fp);
	free(command);
	free(dir_name);
}


/*
	Saves drawn on image to temporary directory.

	{temp_directory}/{video_id}.{frame_number}.png
*/
void save_temp_image(Mat source_image, int video_id, int frame_number, char *temp_directory)
{
	char *cpy_tmp_dir = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);

	strncpy(cpy_tmp_dir, temp_directory, sizeof(char) * READ_BUF_LENGTH);

	char *v_id = (char *) malloc(sizeof(char) * INT_MAX),
			 *f_num = (char *) malloc(sizeof(char) * INT_MAX);

	sprintf(v_id, "%d",video_id);
	sprintf(f_num, "%d",frame_number);

	strcat(cpy_tmp_dir, "/");
	strcat(cpy_tmp_dir, v_id);
	strcat(cpy_tmp_dir, ".");
	strcat(cpy_tmp_dir, f_num);
	strcat(cpy_tmp_dir, ".png");

	imwrite(cpy_tmp_dir, source_image);

	free(cpy_tmp_dir);
	free(v_id);
	free(f_num);

}

/*

	Draw bounding boxes on an image.

	Returns Mat with drawn on image or
					NULL if could not read image at input_filename.

*/
Mat draw_bounding_boxes(char *input_filename, int video_id, int frame_number, PGconn *pgc)
{
	BoundingBox bb;
	Mat source_image;

	source_image = imread(input_filename);
	if(source_image.empty()) {
		// printf("\n\nCould not open source image %s.\n\n", input_filename);
		return source_image;
	}
	
	bb = get_bounding_box("Face", video_id, frame_number, pgc);
	if(bb.x && bb.y && bb.width > 0 && bb.height > 0) draw_rectangle(&source_image, bb, Scalar(255,0,0));

	bb = get_bounding_box("REye", video_id, frame_number, pgc);
	if(bb.x && bb.y && bb.width > 0 && bb.height > 0) draw_rectangle(&source_image, bb, Scalar(0,0,255));
	
	bb = get_bounding_box("LEye", video_id, frame_number, pgc);
	if(bb.x && bb.y && bb.width > 0 && bb.height > 0) draw_rectangle(&source_image, bb, Scalar(0,255,0));
	
	bb = get_bounding_box("Nose", video_id, frame_number, pgc);
	if(bb.x && bb.y && bb.width > 0 && bb.height > 0) draw_rectangle(&source_image, bb, Scalar(0,255,255));

	bb = get_bounding_box("Mouth", video_id, frame_number, pgc);
	if(bb.x && bb.y && bb.width > 0 && bb.height > 0) draw_rectangle(&source_image, bb, Scalar(255,0,255));

	if(DEBUG_IMAGE) {
		imshow("source_image", source_image);
		waitKey(0);	
	}

	return source_image;
}	

/*
	Retrieves bounding box info for bbox_type of video_id at frame_number.
	bbox_type should be string of {"Face", "REye", "LEye", "Nose", "Mouth"}

	Returns BoundingBox if successful or
					NULL if no return.
*/
BoundingBox get_bounding_box(char const *bbox_type, int video_id, int frame_number, PGconn *pgc)
{
	
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	PGresult *res;
	int num_rows;
	int i, j;

	if(PQstatus(pgc) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(pgc));
		PQfinish(pgc);
		exit(EXIT_FAILURE);
	}

	//Construct select statement.
	snprintf(&db_statement[0], MAX_DB_STATEMENT_BUFFER_LENGTH,
					"select upper_left_corner_coord, width, height from bounding_boxes where video_id=%d and frame_number=%d and bbox_type='%s';",
					video_id, frame_number, bbox_type);

	res = PQexec(pgc, &db_statement[0]);

	char *ulc = NULL;
	int w = 0,
			h = 0;

	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			printf("Executed command, no return.\n");
			break;

		case PGRES_TUPLES_OK: 

			num_rows = PQntuples(res);
			if(num_rows < 1) {
				break;
			}
			ulc = PQgetvalue(res, 0, 0);
			w = atoi(PQgetvalue(res, 0, 1));
			h = atoi(PQgetvalue(res, 0, 2));
			
			break;

		default:
			printf("Something went wrong.\n");
			break;
	}


	BoundingBox bb; 
	BoundingBox_init(&bb, bbox_type);

	//If no values were returned, return NULL.
	if(!ulc || !w || !h) {
		return bb;
	}

	int x, y;
	sscanf(ulc, "(%d, %d)", &x, &y);

	bb.x = x;
	bb.y = y;
	bb.width = w;
	bb.height = h;

	PQclear(res);
	return bb;

}

/*
	Helper method to draw bounding box on src_img.  Scalar clr is the color.
*/
void draw_rectangle(Mat *src_img, BoundingBox bb, Scalar clr)
{
	rectangle(*src_img, Point(bb.x, bb.y), Point(bb.x + bb.width, bb.y + bb.height), clr, 1, 8, 0);
}

/*
	Creates video from images in extracted directory using ffmpeg.

	ffmpeg -framerate 29.98 -i temp/9.%d.png -c:v libx264 -pix_fmt yuv420p out.mp4
*/
void make_video(float fps, int video_id, char *dir)
{
	FILE *fp;
	char *command = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);

	snprintf(command, READ_BUF_LENGTH,
					"ffmpeg -loglevel quiet -framerate %f -i %s/%d.%%d.png -c:v libx264 -pix_fmt yuv420p bounding_box_movie_%d.mov",
					fps, dir, video_id, video_id);
	printf("\nCreating Movie: %s\n", command);

	fp = popen(command, "r");
	if(fp == NULL) {
		printf("Could not create video.\n");
	}

	pclose(fp);
}