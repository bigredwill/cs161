/*

Will Simons
CS161
Assignment 3: Determine bounding boxes

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

/*
	Contains struct definition for Bounding Box and methods for manipulating it.
*/
#include "bounding_box.h"

/*
	Modify paths below to your opencv installation
*/
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cv.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/cvaux.h"
#include "/usr/local/Cellar/opencv/2.4.12_2/include/opencv/highgui.h"

/*
	DEBUG_BB flag for displaying bounding boxes.
	DEBUG_PRINT flag for printing debug messages.
*/
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

void get_video_meta(int video_id, int *num_frames, int *frame_width, int *frame_height);
void find_features_in_image(char *input_filename, int video_id, int frame_number);
int insert_bounding_box(BoundingBox *bb, int video_id, int frame_number);
void draw_rectangle(Mat src_img, BoundingBox bb, Scalar clr);
int detect_bounding_box(Mat src_img_gray, BoundingBox *bb, BoundingBox *face_bb, Rect region_rect, double width_multiplier, double height_multiplier, double max_multiplier,double scale_factor, int min_neighbors, CascadeClassifier cascade, int flag);

int main(int argc, char *argv[])
{
	
  char input_filename[1280];
  strcpy (&input_filename[0], "1/0.70.png");

  /*
		Get Input File Directory
  */

	if (argc != 2)
	{
		printf("Program accepts 2 args. ./%s <video_id>\n", argv[0]);
		exit(0);
	}

	int video_id = atoi(argv[1]);
	int num_frames, frame_width, frame_height;

	get_video_meta(video_id, &num_frames, &frame_width, &frame_height);

	printf("Total of %d frames to process for video_id %d.\n", num_frames, video_id);
	int i;
	for (i = 1; i <= num_frames; i++) {

		sprintf(input_filename, "%d/0.%d.png", video_id, i);
		printf("\r\tProgress: %d/%d", i, num_frames); //Carriage return, clears input, shows progress.
		fflush(stdout);

		find_features_in_image(input_filename, video_id, i);
	}
	
	return 0;
}

/*

	Gathers video meta info.

*/
void get_video_meta(int video_id, int *num_frames, int *frame_width, int *frame_height)
{
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];
	PGconn *conn;
	PGresult *res;
	int num_rows;
	int i, j;

	snprintf(&conninfo[0], MAX_DB_STATEMENT_BUFFER_LENGTH,"dbname = '%s' host = '%s' port = '%s' user = '%s' password = '%s'",
				DBNAME, HOST, PORT, USER, PASSWORD);
	conn = PQconnectdb(conninfo);


	if(PQstatus(conn) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(EXIT_FAILURE);
	}

	snprintf(&db_statement[0], MAX_DB_STATEMENT_BUFFER_LENGTH, "select num_frames, x_resolution, y_resolution from video_meta where video_id=%d;",
					video_id);

	res = PQexec(conn, &db_statement[0]);

	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			printf("Executed command, no return.\n");
			break;

		case PGRES_TUPLES_OK: 

			num_rows = PQntuples(res);

			*num_frames = atoi(PQgetvalue(res, 0, 0));
			*frame_width = atoi(PQgetvalue(res, 0, 1));
			*frame_height = atoi(PQgetvalue(res, 0, 2));
			
			break;

		default:
			printf("Something went wrong.\n");
			break;
	}

	PQclear(res);
	PQfinish(conn);
}



/*

	Finds features in an image, then writes calls insert_bounding_box
	to insert all bounding boxes from image.

*/

void find_features_in_image(char *input_filename, int video_id, int frame_number)
{
	Mat source_image,
			source_image_gray,
			source_image_gray_face_region,
			source_image_gray_left_eye_region,
			source_image_gray_right_eye_region,
			source_image_gray_nose_region,
			source_image_gray_mouth_region;
  
  CascadeClassifier face_cascade,
  									left_eye_cascade,
  									right_eye_cascade,
  									nose_cascade,
  									mouth_cascade;

  std::vector<Rect> face, eyes, nose, mouth;

  int min_neighbors;
  double scale_factor;
  min_neighbors = 10;
  scale_factor  = 1.01;

  BoundingBox *face_bb = BoundingBox_new();
  BoundingBox *right_eye_bb = BoundingBox_new();
  BoundingBox *left_eye_bb = BoundingBox_new();
  BoundingBox *nose_bb = BoundingBox_new();
  BoundingBox *mouth_bb = BoundingBox_new();


  if(face_cascade.load ("haarcascade_frontalface_alt2.xml") 
  		&& left_eye_cascade.load ("ojoI.xml")
  		&& right_eye_cascade.load ("ojoD.xml")
  		&& nose_cascade.load ("Nariz.xml")
  		&& mouth_cascade.load ("Mouth.xml")) 
  {
  	
  	BoundingBox_init(face_bb, "Face");
  	BoundingBox_init(right_eye_bb, "REye");
  	BoundingBox_init(left_eye_bb, "LEye");
  	BoundingBox_init(nose_bb, "Nose");
  	BoundingBox_init(mouth_bb, "Mouth");

  	// int video_id = 0;
  	// int frame_number = 0;


  	source_image_gray = imread(input_filename, CV_LOAD_IMAGE_GRAYSCALE);
  	if(!source_image_gray.empty()) {
  		equalizeHist(source_image_gray, source_image_gray);
  		face_cascade.detectMultiScale(source_image_gray, face, scale_factor, min_neighbors, CV_HAAR_FIND_BIGGEST_OBJECT);
  		if(face.size() > 0) {
  			
  			face_bb->x = face[0].x;
  			face_bb->y = face[0].y;
  			face_bb->width = face[0].width;
  			face_bb->height = face[0].height;

  			insert_bounding_box(face_bb, video_id, frame_number);


				Rect left_eye_region_rect = Rect(face_bb->x + face_bb->width/2, face_bb->y + face_bb->height/5.5, face_bb->width/2, face_bb->height/3);
				if(1 == detect_bounding_box(source_image_gray, left_eye_bb, face_bb, left_eye_region_rect, 0.18, 0.24, 2, scale_factor, min_neighbors, left_eye_cascade, CV_HAAR_FIND_BIGGEST_OBJECT)) {
					insert_bounding_box(left_eye_bb, video_id, frame_number);
				}

				Rect right_eye_region_rect = Rect(face_bb->x, face_bb->y + face_bb->height/5.5, face_bb->width/2, face_bb->height/3);
				if(1 == detect_bounding_box(source_image_gray, right_eye_bb, face_bb, right_eye_region_rect, 0.18, 0.24, 2, scale_factor, min_neighbors, right_eye_cascade, CV_HAAR_FIND_BIGGEST_OBJECT)) {
					insert_bounding_box(right_eye_bb, video_id, frame_number);
				}

				Rect nose_region_rect = Rect(face_bb->x, face_bb->y + face_bb->height/3, face_bb->width, face_bb->height/3);
				if(1 == detect_bounding_box(source_image_gray, nose_bb, face_bb, nose_region_rect, 0.25, 0.15, 2, scale_factor, min_neighbors, nose_cascade, CV_HAAR_FIND_BIGGEST_OBJECT)) {
					insert_bounding_box(nose_bb, video_id, frame_number);
				}

				Rect mouth_region_rect = Rect(face_bb->x, face_bb->y + face_bb->height/2, face_bb->width, face_bb->height/2);
				if(1 == detect_bounding_box(source_image_gray, mouth_bb, face_bb, mouth_region_rect, 0.25, 0.15, 5,scale_factor, min_neighbors, mouth_cascade, CV_HAAR_FIND_BIGGEST_OBJECT)) {
					insert_bounding_box(mouth_bb, video_id, frame_number);
				}
  		}
  	}


  }

  //only drawing output if DEBUG_BB flag is set
  if(DEBUG_BB) {

  	source_image = imread(input_filename);

  	draw_rectangle(source_image, *face_bb, Scalar(0,0,0));
  	draw_rectangle(source_image, *left_eye_bb, Scalar(255,255,255));
  	draw_rectangle(source_image, *right_eye_bb, Scalar(255,255,255));
  	draw_rectangle(source_image, *nose_bb, Scalar(0,255,0));
  	draw_rectangle(source_image, *mouth_bb, Scalar(0,0,255));


  	imshow ("Image", source_image);
    waitKey (0); 
  }

  
  BoundingBox_destroy(face_bb);
  BoundingBox_destroy(right_eye_bb);
  BoundingBox_destroy(left_eye_bb);
  BoundingBox_destroy(nose_bb);
  BoundingBox_destroy(mouth_bb);
}

/*

	Inserts Bounding Box info into Postgres database.

*/
int insert_bounding_box(BoundingBox *bb, int video_id, int frame_number)
{
	if(DEBUG_PRINT) {
		printf("insert_bounding_box\n");
		BoundingBox_print(bb);
		printf("video_id: %d, frame_number: %d\n", video_id, frame_number);	
	}
	// return 1;
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];
	PGconn *conn;
	PGresult *res;
	int num_rows;
	int i, j;

	snprintf(&conninfo[0], MAX_DB_STATEMENT_BUFFER_LENGTH,"dbname = '%s' host = '%s' port = '%s' user = '%s' password = '%s'",
				DBNAME, HOST, PORT, USER, PASSWORD);
	conn = PQconnectdb(conninfo);


	if(PQstatus(conn) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(EXIT_FAILURE);
	}

	snprintf(&db_statement[0], MAX_DB_STATEMENT_BUFFER_LENGTH, "INSERT INTO bounding_boxes (video_id, frame_number, bbox_type, upper_left_corner_coord, width, height) VALUES (%d, %d, '%s', point(%d,%d), %d, %d);",
						video_id, frame_number, bb->name, bb->x, bb->y, bb->width, bb->height);


	res = PQexec(conn, &db_statement[0]);

	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			// printf("Executed command, no return.\n");
			break;

		default:
			printf("Something went wrong.\n");
			break;
	}

	PQclear(res);
	PQfinish(conn);
	return 1;
	
	return 1;
}

/*
	
	Attempts to find a bouding box for a CascadeClassifier in a Rect region from Mat src_img_gray.
	Updates info in BoundingBox bb pointer.

	
	return 1 if bounding box is found
	return 0 if bounding box is not found.
*/
int detect_bounding_box(Mat src_img_gray, BoundingBox *bb, BoundingBox *face_bb, Rect region_rect, double width_multiplier, double height_multiplier, double max_multiplier,double scale_factor, int min_neighbors, CascadeClassifier cascade, int flag)
{
	Mat source_image_gray_region;
	std::vector<Rect> area;

	bb->min_width = width_multiplier * face_bb->width;
	bb->min_height = height_multiplier * face_bb->height;

	if(bb->min_width < (100*width_multiplier) || bb->min_height < (100*height_multiplier)) {
		bb->min_width = 100*width_multiplier;
		bb->min_height = 100*height_multiplier;
	}

	bb->max_width = bb->min_width * max_multiplier;
	bb->max_height = bb->min_height * max_multiplier;

	source_image_gray_region = src_img_gray(region_rect);

	Size sizeMin = Size(bb->min_width, bb->min_height);
	Size sizeMax = Size(bb->max_width, bb->max_height);


	cascade.detectMultiScale(source_image_gray_region, area, scale_factor, min_neighbors, flag);
	if(area.size() > 0) {
		bb->x = region_rect.x + area[0].x;
		bb->y = region_rect.y + area[0].y;
		bb->width = area[0].width;
		bb->height = area[0].height;
		return 1;
	} else {
		printf("Nothing Detected\n");
		return 0;
	}

}

/*
	Helper method to draw bounding box on src_img.  Scalar clr is the color.
*/
void draw_rectangle(Mat src_img, BoundingBox bb, Scalar clr)
{
	rectangle(src_img, Point(bb.x, bb.y), Point(bb.x + bb.width, bb.y + bb.height), clr, 1, 8, 0);
}

