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
cv::Rect get_bounding_box(char const *bbox_type, int video_id, int frame_number, PGconn *pgc);
void draw_rectangle(cv::Mat *src_img, cv::Rect bb, CvScalar clr);
int database_insert_pupil(int video_id, int frame_number, cv::Point rEyePupil, cv::Point lEyePupil, PGconn *conn);

int main(int argc, char *argv[])
{
	char input_filename[READ_BUF_LENGTH], imgDirectory[READ_BUF_LENGTH];
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
	// video_id = atoi(argv[1]);

	if(video_id <= 0) {
		printf("Please enter a valid (int > 0) video_id number.\n");
		exit(1);
	}
	printf("Finding Pupils for video_id=%d\n", video_id);

	conn = database_connect();
	//get video meta data
	int res = get_video_meta(video_id, &num_frames, &frame_width, &frame_height, &fps, conn);
	if(res == 0) {
		printf("No entry for video_id=%d exists in database!\n", video_id);
		database_disconnect(conn);
		exit(1);
	}

	snprintf(imgDirectory, READ_BUF_LENGTH, "./images/%d", video_id);
	// cv::Rect face;
	if(does_dir_exist(imgDirectory) == 1) {
		
		for(i = 1; i <= num_frames; i++) {

			printf("\rFrames Processed %d/%d", i, num_frames);
	    fflush(stdout);

			snprintf(input_filename, READ_BUF_LENGTH, "%s/0.%d.png", imgDirectory, i);
			source_image = cv::imread(input_filename);
			
			if(!source_image.empty()) {

				//convert image to gray scale
				std::vector<cv::Mat> rgbChannels(3);
			  cv::split(source_image, rgbChannels);
			  cv::Mat frame_gray = rgbChannels[2];

			  //reset variables
				rEyeCenter = cv::Point(0,0);
				lEyeCenter = cv::Point(0,0);

				//check if left_eye_bb
				bb = get_bounding_box("LEye", video_id, i, conn);
				if(bb.x && bb.y && bb.width > 0 && bb.height > 0) {
					lEyeCenter = findEyeCenter(frame_gray, bb, "");
					//adjust to image coordinates
					lEyeCenter.x = lEyeCenter.x + bb.x;
					lEyeCenter.y = lEyeCenter.y + bb.y;
					
					// circle(source_image, lEyeCenter, 3, 1234);
				}

				//check if right_eye_bb
				bb = get_bounding_box("REye", video_id, i, conn);
				if(bb.x && bb.y && bb.width > 0 && bb.height > 0) {
					rEyeCenter = findEyeCenter(frame_gray, bb, "");
					//adjust to image coordinates
					rEyeCenter.x = rEyeCenter.x + bb.x;
					rEyeCenter.y = rEyeCenter.y + bb.y;
					
					// circle(source_image, rEyeCenter, 3, 1234);
				}

				// imshow("pupils", source_image);
				// cv::waitKey();

				//write to database
				database_insert_pupil(video_id, i, rEyeCenter, lEyeCenter, conn);
			}
		}

		printf("\nDone\n");

	} else {
		printf("Image directory does not exist.\n");
	}
				
	database_disconnect(conn);
	return 0;
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

int database_insert_pupil(int video_id, int frame_number, cv::Point rEyePupil, cv::Point lEyePupil, PGconn *conn)
{
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];
	
	PGresult *res;
	int num_rows;
	int i, j;

	if(PQstatus(conn) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(EXIT_FAILURE);
	}

	snprintf(db_statement, MAX_DB_STATEMENT_BUFFER_LENGTH,
		"INSERT INTO pupils (video_id, frame_number, left_eye_coords, right_eye_coords) VALUES (%d, %d, point(%d,%d), point(%d,%d));",
		video_id, frame_number, lEyePupil.x, lEyePupil.y, rEyePupil.x, rEyePupil.y);

	res = PQexec(conn, &db_statement[0]);
	return 1;
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
cv::Rect get_bounding_box(char const *bbox_type, int video_id, int frame_number, PGconn *pgc)
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

	cv::Rect bb;

	//If no values were returned, return NULL.
	if(!ulc || !w || !h) {
		bb = cv::Rect(0,0,0,0);
		return bb;
	}


	int x, y;
	sscanf(ulc, "(%d, %d)", &x, &y);
	bb = cv::Rect(x,y,w,h);

	PQclear(res);
	return bb;

}

/*
	Helper method to draw bounding box on src_img.  Scalar clr is the color.
*/
void draw_rectangle(cv::Mat *src_img, cv::Rect bb, CvScalar clr)
{
	rectangle(*src_img, cv::Point(bb.x, bb.y), cv::Point(bb.x + bb.width, bb.y + bb.height), clr, 1, 8, 0);
}
