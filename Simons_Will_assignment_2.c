/*
	Will Simons
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <sys/stat.h>
#include <sys/types.h>

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

void clear_buffer(char *buf);
void pipe_to_buffer(FILE *pipe, char *buf, char *cmmd, char const *video_path);
void extract_meta(char const *video_path, int *width, int *height, int *fcount, float *fps);
void extract_stills(char const *video_path, char *dir, float fps);
char *insert_video_meta(char const *name, int width, int height, int fcount, float fps);

int main(int argc, char const *argv[])
{
	//Initialize variables
	int width, height, fcount;
	float fps;

	/*
	Check if video file location was passed in.
	*/
	if(argc != 2) {
		printf("Program accepts 2 args. ./%s <path/to/video/file> \n", argv[0]);
		return 1;
	}

	//help
	if (strcmp(argv[0], "-h") == 0)
	{
		printf("Extracts still images from video file to directory named by unique id given to video in database.");
		printf("\n%s <path/to/video/file>\n", argv[0]);
		return 1;
	}

	//Extracts meta from video into width, height, f(rame)count, fps variables
	extract_meta(argv[1], &width, &height, &fcount, &fps);
	//Insert Video Metadata into database. 
  char *unique_id = insert_video_meta(argv[1], width, height, fcount, fps);
	//Extract still images from video into directory named by unique_id
  char dir[1024] = "images/";
  strcat(dir, unique_id);
	extract_stills(argv[1], dir, fps);

	return 0;
}


/*
	Inserts video metadata into database.
	Connects to database
*/
char *insert_video_meta(char const *name, int width, int height, int fcount, float fps) {
	char db_statement[MAX_DB_STATEMENT_BUFFER_LENGTH];
	char conninfo[MAX_DB_STATEMENT_BUFFER_LENGTH];
	PGconn *conn;
	PGresult *res;
	int num_rows;
	int i, j;
	char *unique_id;

	// sprintf(unique_id, "-1");

	snprintf(&conninfo[0], MAX_DB_STATEMENT_BUFFER_LENGTH,"dbname = '%s' host = '%s' port = '%s' user = '%s' password = '%s'",
				DBNAME, HOST, PORT, USER, PASSWORD);
	conn = PQconnectdb(conninfo);

	if(PQstatus(conn) != CONNECTION_OK) 
	{
		printf("Connetion to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(EXIT_FAILURE);
	}

	snprintf(&db_statement[0], MAX_DB_STATEMENT_BUFFER_LENGTH, "INSERT INTO video_meta (name, num_frames, x_resolution, y_resolution, fps) VALUES ('%s', %d, %d, %d, %f) RETURNING video_id;",
					name, fcount, width, height, fps);

	res = PQexec(conn, &db_statement[0]);

	switch (PQresultStatus(res)) {
		case PGRES_COMMAND_OK: 
			printf("Executed command, no return.\n");
			break;

		case PGRES_TUPLES_OK: 
			num_rows = PQntuples(res);
			unique_id = PQgetvalue(res, 0, 0);
			break;

		default:
			printf("Something went wrong.\n");
			break;
	}

	PQfinish(conn);
	return unique_id;

}


/*
Clears char* buffer.
*/
void clear_buffer(char *buf) 
{
	int toClear = strlen(buf);
	memset(&buf[0], 0, sizeof(buf));
}


/*
	Executes cmmd through popen and returns output to buffer.
*/
void pipe_to_buffer(FILE *pipe, char *buf, char *cmmd, char const *vid)
{
	strcat(cmmd, vid);
	if ((pipe = popen(cmmd, "r")) == NULL) {
		perror("popen");
		exit(1);
	}

	fgets(buf, 80, pipe);

	pclose(pipe);
}

/*
	Extract Meta information from video using ffprobe.
*/
void extract_meta(char const *video_path, int *width, int *height, int *fcount, float *fps)
{
	FILE *pipe_fp;
	char readbuf[80]; //todo: magic number, not good
	char fcount_probe_cmmd[1024] = "ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of default=nokey=1:noprint_wrappers=1 ";
	char fps_probe_cmmd[1024] = "ffprobe -v error -select_streams v:0 -show_entries stream=avg_frame_rate -of default=noprint_wrappers=1:nokey=1 ";
	char y_probe_cmmd[1024] = "ffprobe -v error -of flat=s=_ -select_streams v:0 -show_entries stream=height ";
	char x_probe_cmmd[1024] = "ffprobe -v error -of flat=s=_ -select_streams v:0 -show_entries stream=width ";

	/*
	Extract frame count information from video.
	*/

	pipe_to_buffer(pipe_fp, readbuf, fcount_probe_cmmd, video_path);

	if(sscanf(readbuf, "%d", fcount) != 1) {
		// printf("Frame Count could not be read.\n");
	}
	printf("Frame Count: %d\n", *fcount);
	clear_buffer(readbuf);


	/*
	Extract fps information from video.
	*/
	pipe_to_buffer(pipe_fp, readbuf, fps_probe_cmmd, video_path);

	int _n, _d; //must extract 
	if(sscanf(readbuf, "%d/%d", &_n, &_d) != 1) {
		// printf("FPScould not be read.\n");
	}

	float _f = ((float) _n / (float) _d);
	memcpy(fps, &_f, sizeof(float));

	printf("FPS (%d/%d) : %f\n", _n, _d, *fps);
	clear_buffer(readbuf);

	/*
	Extract height information from video.
	*/
	pipe_to_buffer(pipe_fp, readbuf, y_probe_cmmd, video_path);

	if(sscanf(readbuf, "streams_stream_0_height=%d", height) != 1) {
		// printf("FPScould not be read.\n");
	}
	printf("Height: %d\n", *height);
	clear_buffer(readbuf);

	/*
	Extract width information from video.
	*/
	pipe_to_buffer(pipe_fp, readbuf, x_probe_cmmd, video_path);
	
	if(sscanf(readbuf, "streams_stream_0_width=%d", width) != 1) {
		// printf("FPScould not be read.\n");
	}
	printf("Width: %d\n", *width);
	clear_buffer(readbuf);
}


/*
Calls ffmpeg to extract stills from video.
*/
void extract_stills(char const *video_path, char *dir, float fps) {
	FILE *pipe_fp;

	struct stat st = {0};
	//check if directory exists already
	if(stat(dir, &st) == -1) {
		mkdir(dir, 0700);
		printf("Created directory %s\n", dir);
	} else {
		printf("Directory %s already exists, could not create directory.\n", dir);
		pclose(pipe_fp);
		exit(1);
	}

	//constructing this `ffmpeg -i IMG_2461.MOV -vf fps=29.982466 testout/out\%d.png"`
	char extract_frames_cmmd[2048] = "ffmpeg -i ";
	
	//construct ffmpeg command 
	strcat(extract_frames_cmmd, video_path);

	strcat(extract_frames_cmmd, " -vf fps=");
 
	//convert fps (float) to string
	char str_fps[100];
	sprintf(str_fps, "%f ", fps);
	strcat(extract_frames_cmmd, str_fps);

	strcat(extract_frames_cmmd, dir);
	strcat(extract_frames_cmmd, "/0.\%d.png");

	printf("%s\n", extract_frames_cmmd);

	//execute command then close pipe
	if ((pipe_fp = popen(extract_frames_cmmd, "r")) == NULL) {
		perror("popen");
		exit(1);
	}
	pclose(pipe_fp);
}

