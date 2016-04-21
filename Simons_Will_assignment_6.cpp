#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libpq-fe.h>

#include "findEyeCenter.h"

/*
	Modify paths below to your opencv installation
*/
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cv.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cvaux.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/highgui.h"


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

PGconn *database_connect(); //Returns new connection to database
int database_disconnect(PGconn *conn); //Closes connection to database
int does_dir_exist(char *dir_name);
int get_video_meta(int video_id, int *num_frames, int *frame_width, int *frame_height, float *fps, PGconn *pgc);
void get_eye_pupils(cv::Point *lEyeCenter, cv::Point *rEyeCenter, int video_id, int frame_number, PGconn *pgc);
void save_temp_image(cv::Mat source_image, char *temp_directory, int video_id, int frame_number);
void create_temp_directory(char *temp_directory);
void remove_temp_directory(char *temp_directory);
void make_video(float fps, int video_id, char *dir);

int main(int argc, char *argv[])
{
	char input_filename[READ_BUF_LENGTH], imgDirectory[READ_BUF_LENGTH], tempImgDirectory[READ_BUF_LENGTH];
	int video_id, num_frames, frame_width, frame_height, i;
	float fps;
	PGconn *conn;
	cv::Mat source_image;
	cv::Point lEyeCenter, rEyeCenter;
	cv::Rect bb;

	if (argc < 2) {
		printf("Program accepts 2 args. ./%s <video_id>\n", argv[0]);
		exit(1);
	}

	video_id = (int)strtol(argv[1], (char **)NULL, 10);

	if(video_id <= 0) {
		printf("Please enter a valid (int > 0) video_id number.\n");
		exit(1);
	}
	printf("Creating pupils video for video_id=%d\n", video_id);

	conn = database_connect();
	//get video meta data
	int res = get_video_meta(video_id, &num_frames, &frame_width, &frame_height, &fps, conn);
	if(res == 0) {
		printf("No entry for video_id=%d exists in database!\n", video_id);
		database_disconnect(conn);
		exit(1);
	}

	snprintf(imgDirectory, READ_BUF_LENGTH, "./images/%d", video_id);
	snprintf(tempImgDirectory, READ_BUF_LENGTH, "temp_pupils_%d", video_id);

	if(does_dir_exist(imgDirectory) == 1) {
		create_temp_directory(tempImgDirectory);	

		for(i = 1; i <= num_frames; i++) {
			printf("\r\tFrames Processed %d/%d", i, num_frames);
	    fflush(stdout);

			snprintf(input_filename, READ_BUF_LENGTH, "%s/0.%d.png", imgDirectory, i);
			source_image = cv::imread(input_filename);
			
			if(!source_image.empty()) {

			  //reset variables
				rEyeCenter.x = -1;
				rEyeCenter.y = -1;
				lEyeCenter.x = -1;
				lEyeCenter.y = -1;

				//check if left_eye_bb
				get_eye_pupils(&lEyeCenter, &rEyeCenter, video_id, i, conn);

				if(lEyeCenter.x >= 0 && lEyeCenter.x >= 0) {
					line(source_image, cv::Point(lEyeCenter.x - 5, lEyeCenter.y), cv::Point(lEyeCenter.x + 5, lEyeCenter.y), cvScalar(0,255,255), 2);
					line(source_image, cv::Point(lEyeCenter.x, lEyeCenter.y - 5), cv::Point(lEyeCenter.x, lEyeCenter.y + 5), cvScalar(0,255,255), 2);
				}
				if(rEyeCenter.x >= 0 && rEyeCenter.x >= 0) {
					line(source_image, cv::Point(rEyeCenter.x - 5, rEyeCenter.y), cv::Point(rEyeCenter.x + 5, rEyeCenter.y), cvScalar(0,255,255), 2);
					line(source_image, cv::Point(rEyeCenter.x, rEyeCenter.y - 5), cv::Point(rEyeCenter.x, rEyeCenter.y + 5), cvScalar(0,255,255), 2);
				}

				// imshow("pupils", source_image);
				// cv::waitKey();

				//write image to temporary directory
				save_temp_image(source_image, tempImgDirectory, video_id, i);
			}
		}
		make_video(fps, video_id, tempImgDirectory);
		remove_temp_directory(tempImgDirectory);
		printf("\n\tDone\n");

	} else {
		printf("Image directory does not exist.\n");
	}
				
	database_disconnect(conn);
	return 0;
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
					"ffmpeg -loglevel quiet -framerate %f -i %s/%d.%%d.png -c:v libx264 -pix_fmt yuv420p eye_pupil_tracking_movie_%d.mov",
					fps, dir, video_id, video_id);
	printf("\nCreating Movie: %s\n", command);

	fp = popen(command, "r");
	if(fp == NULL) {
		printf("Could not create video.\n");
	}

	pclose(fp);
}

/*
	Saves drawn on image to temporary directory.

	{temp_directory}/{video_id}.{frame_number}.png
*/
void save_temp_image(cv::Mat source_image, char *temp_directory, int video_id, int frame_number) {

	char *cpy_tmp_dir = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);
	snprintf(cpy_tmp_dir, READ_BUF_LENGTH, "%s/%d.%d.png", temp_directory, video_id, frame_number);
	// printf(cpy_tmp_dir);
	imwrite(cpy_tmp_dir, source_image);
	free(cpy_tmp_dir);
}

/*
	Creates temporary directory.
	Returns name of directory.
*/
void create_temp_directory(char *temp_directory) {
	// char *dir = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);
	// sprintf(dir, "temp");

	struct stat st = {0};
	if(stat(temp_directory, &st) == -1) {
		mkdir(temp_directory, 0700);
		printf("\tCreated temp directory: %s\n", temp_directory);
	} else {
		printf("\tCouldn't create dir\n");
	}

	// printf("\t%s", temp_directory);
}


/*
	Remove directory with path dir_name.
*/
void remove_temp_directory(char *temp_directory) {
	FILE *fp;
	char *command = (char *) malloc(sizeof(char) * READ_BUF_LENGTH);

	sprintf(command, "rm -r ");
	snprintf(command, READ_BUF_LENGTH, "rm -r %s", temp_directory);
	// strlcat(command, temp_directory, READ_BUF_LENGTH);

	fp = popen(command, "r");
	if( fp == NULL ) {
		printf("\n\tCouldn't remove directory %s\n", temp_directory);
	} else {
		printf("\tRemoved Temp Directory.\n");
	}

	pclose(fp);
	free(command);
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
		printf("Connection to database failed: %s\n", PQerrorMessage(pgc));
		PQfinish(pgc);
		exit(0);
	} else {
		return pgc;
	}
}

int database_disconnect(PGconn *conn)
{
	PQfinish(conn);
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
	Retrieves bounding box info for bbox_type of video_id at frame_number.
	bbox_type should be string of {"Face", "REye", "LEye", "Nose", "Mouth"}

	Returns BoundingBox if successful or
					NULL if no return.
*/
void get_eye_pupils(cv::Point *lEyeCenter, cv::Point *rEyeCenter, int video_id, int frame_number, PGconn *pgc)
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
					"select left_eye_coords, right_eye_coords from pupils where video_id=%d and frame_number=%d;",
					video_id, frame_number);
	// printf("%s\n", db_statement);

	res = PQexec(pgc, &db_statement[0]);

	char *left_pupil = NULL;
	char *right_pupil = NULL;


	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			printf("Executed command, no return.\n");
			break;

		case PGRES_TUPLES_OK: 

			num_rows = PQntuples(res);
			if(num_rows < 1) {
				break;
			}
			left_pupil = PQgetvalue(res, 0, 0);
			right_pupil = PQgetvalue(res, 0, 1);
			
			break;

		default:
			printf("Something went wrong.\n");
			break;
	}
	int x, y;
	if(left_pupil) {
		sscanf(left_pupil, "(%d, %d)", &x, &y);
		lEyeCenter->x = x;
		lEyeCenter->y = y;
		// printf("%s\t", left_pupil);
	} else {
		// printf("Nothing\t");
	}

	if(right_pupil) {
		sscanf(right_pupil, "(%d, %d)", &x, &y);
		rEyeCenter->x = x;
		rEyeCenter->y = y;
		// printf("%s\n", left_pupil);
	} else {
		// printf("Nothing\n");
	}

	PQclear(res);
}

