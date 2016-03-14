/*

Will Simons
CS161
Assignment 3: Determine bounding boxes

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

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

PGconn *conn; //Database connection to keep open.

Mat draw_bounding_boxes(char *input_filename, int video_id, int frame_number);
BoundingBox get_bounding_box(char const *bbox_type, int video_id, int frame_number);
void draw_rectangle(Mat *source_image, BoundingBox bb, Scalar clr);
int get_video_meta(int video_id, int *num_frames, int *frame_width, int *frame_height, float *fps);
void save_temp_image(Mat source_image, int video_id, int frame_number, char *temp_directory);
char *create_temp_directory(int video_id);
void remove_temp_directory(char *temp_directory);
void make_video(float fps, int video_id, char *dir);
int database_connect();
int database_disconnect();
int does_dir_exist(char *dir_name);

int main(int argc, char *argv[]) {

	char input_filename[1280];
	char *temp_directory;
	int video_id, num_frames, frame_width, frame_height, i, ret;
	float fps;
	Mat source_image;

	if (argc != 2) {
		printf("Program accepts 2 args. ./%s <video_id>\n", argv[0]);
		exit(0);
	}

	video_id = atoi(argv[1]);

	if(video_id < 0) {
		printf("Please enter a positive video_id number.\n");
	}

	//Open Database connection.
	if(database_connect() != 1) {
		printf("Could not connect to database\n");
		exit(0);
	}

	ret = get_video_meta(video_id, &num_frames, &frame_width, &frame_height, &fps);
	if(!ret) {
		printf("Video with id %d does not exist.\n", video_id);
		database_disconnect();
		exit(0);
	}

	char *img_dir = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);
	snprintf(img_dir, READ_BUF_LENGTH, "images/%d", video_id);
	ret = does_dir_exist(img_dir);
	if(!ret) {
		printf("No directory at %s containing images exists. Make sure you extract images first (Assignment 2).\n", img_dir);
		database_disconnect();
		exit(0);
	}

	temp_directory = create_temp_directory(video_id);

	printf("Processing video %d with %d frames, width %d, height %d, at %.2f fps.\n", video_id, num_frames, frame_width, frame_height, fps);

	for(i = 1; i <= num_frames; i++) {

		snprintf(input_filename, READ_BUF_LENGTH, "images/%d/0.%d.png", video_id, i);
		printf("\r\tProgress: %d/%d", i, num_frames);
		fflush(stdout);

		source_image = draw_bounding_boxes(input_filename, video_id, i);

		if(!source_image.empty()) {
			save_temp_image(source_image, video_id, i, temp_directory);
		} else {
			printf("Image Empty");
		}
		
	}

	database_disconnect();
	make_video(fps, video_id, temp_directory);
	remove_temp_directory(temp_directory);

	printf("\nFinished Processing\n");

}

//ffmpeg -framerate 29.98 -i temp/9.%d.png -c:v libx264 -pix_fmt yuv420p out.mp4
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
char *create_temp_directory(int video_id) {
	char *dir = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);
	sprintf(dir, "temp");

	char *v_id = (char *) malloc(sizeof(char) * 32); //max size of 32 bit integer.
	sprintf(v_id, "%d",video_id);
	free(v_id);

	struct stat st = {0};
	if(stat(dir, &st) == -1) {
		mkdir(dir, 0700);
		printf("\tCreated temp directory: %s", dir);
	}

	printf("\t%s", dir);
	return dir;
}

/*
	Remove directory with name.
*/
void remove_temp_directory(char *dir_name) {
	FILE *fp;
	char *command = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);

	sprintf(command, "rm -r ");
	strlcat(command, dir_name, READ_BUF_LENGTH);

	fp = popen(command, "r");
	if( fp == NULL ) {
		printf("\nCouldn't remove directory %s\n", dir_name);
	} else {
		printf("Removed Temp Directory.\n");
	}

	pclose(fp);
	free(command);
	free(dir_name);
}

/*
	Establishes connection to database.
	Returns: 1 on success, 0 on failure.
*/
int database_connect() {
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];

	snprintf(&conninfo[0], MAX_DB_STATEMENT_BUFFER_LENGTH,"dbname = '%s' host = '%s' port = '%s' user = '%s' password = '%s'",
				DBNAME, HOST, PORT, USER, PASSWORD);
	conn = PQconnectdb(conninfo);
	if(PQstatus(conn) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		return 0;
	} else {
		return 1;
	}
}

/*
	Closes the connection to database.
*/
int database_disconnect() {
	PQfinish(conn);
	return 1;
}

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

	// printf("\t%s", cpy_tmp_dir);
	imwrite(cpy_tmp_dir, source_image);

	free(cpy_tmp_dir);
	free(v_id);
	free(f_num);

}

Mat draw_bounding_boxes(char *input_filename, int video_id, int frame_number)
{
	BoundingBox bb;
	Mat source_image;

	source_image = imread(input_filename);
	if(source_image.empty()) {
		printf("Could not open source image %s.\n", input_filename);
		exit(0);
	}
	
	bb = get_bounding_box("Face", video_id, frame_number);
	draw_rectangle(&source_image, bb, Scalar(255,0,0));

	bb = get_bounding_box("REye", video_id, frame_number);
	draw_rectangle(&source_image, bb, Scalar(0,0,255));

	bb = get_bounding_box("LEye", video_id, frame_number);
	draw_rectangle(&source_image, bb, Scalar(0,255,0));

	bb = get_bounding_box("Nose", video_id, frame_number);
	draw_rectangle(&source_image, bb, Scalar(0,255,255));

	bb = get_bounding_box("Mouth", video_id, frame_number);
	draw_rectangle(&source_image, bb, Scalar(255,0,255));

	if(DEBUG_IMAGE) {
		imshow("source_image", source_image);
		waitKey(0);	
	}

	return source_image;
}	

BoundingBox get_bounding_box(char const *bbox_type, int video_id, int frame_number) {
	
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	// char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];
	
	PGresult *res;
	int num_rows;
	int i, j;

	// snprintf(&conninfo[0], MAX_DB_STATEMENT_BUFFER_LENGTH,"dbname = '%s' host = '%s' port = '%s' user = '%s' password = '%s'",
	// 			DBNAME, HOST, PORT, USER, PASSWORD);
	// conn = PQconnectdb(conninfo);

	if(PQstatus(conn) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(EXIT_FAILURE);
	}


	snprintf(&db_statement[0], MAX_DB_STATEMENT_BUFFER_LENGTH,
					"select upper_left_corner_coord, width, height from bounding_boxes where video_id=%d and frame_number=%d and bbox_type='%s';",
					video_id, frame_number, bbox_type);

	res = PQexec(conn, &db_statement[0]);

	char *ulc;
	int w, h;

	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			printf("Executed command, no return.\n");
			break;

		case PGRES_TUPLES_OK: 

			num_rows = PQntuples(res);

			ulc = PQgetvalue(res, 0, 0);
			w = atoi(PQgetvalue(res, 0, 1));
			h = atoi(PQgetvalue(res, 0, 2));
			
			break;

		default:
			printf("Something went wrong.\n");
			break;
	}

	
	int x, y;
	sscanf(ulc, "(%d, %d)", &x, &y);
	
	BoundingBox bb; 
	BoundingBox_init(&bb, bbox_type);

	bb.x = x;
	bb.y = y;
	bb.width = w;
	bb.height = h;

	PQclear(res);
	// PQfinish(conn);
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

	Gathers video meta info.

*/
int get_video_meta(int video_id, int *num_frames, int *frame_width, int *frame_height, float *fps)
{
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];
	PGresult *res;
	int num_rows;
	int i, j;

	//check that connection is still alive
	if(PQstatus(conn) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(EXIT_FAILURE);
	}

	snprintf(&db_statement[0], MAX_DB_STATEMENT_BUFFER_LENGTH, "select num_frames, x_resolution, y_resolution, fps from video_meta where video_id=%d;",
					video_id);

	res = PQexec(conn, &db_statement[0]);

	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			printf("Executed command, no return.\n");
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