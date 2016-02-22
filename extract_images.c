#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define READ_BUF_LENGTH 1024


void clear_buffer(char *buf);
void readToBuf(FILE *pipe, char *buf, char *cmmd, char const *video_path);
void extractMeta(char const *video_path, int *width, int *height, int *fcount, float *fps);
void extractStills(char const *video_path, char *dir, float fps);

int main(int argc, char const *argv[])
{

	FILE *pipe_fp;
	char readbuf[80];
	char fcount_probe_cmmd[1024] = "ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of default=nokey=1:noprint_wrappers=1 ";
	char fps_probe_cmmd[1024] = "ffprobe -v error -select_streams v:0 -show_entries stream=avg_frame_rate -of default=noprint_wrappers=1:nokey=1 ";
	char y_probe_cmmd[1024] = "ffprobe -v error -of flat=s=_ -select_streams v:0 -show_entries stream=height ";
	char x_probe_cmmd[1024] = "ffprobe -v error -of flat=s=_ -select_streams v:0 -show_entries stream=width ";

	int width, height, fcount;
	float fps;

	/*
	Check if video file location was passed in.
	*/
	if(argc != 2) {
		printf("Program accepts 2 args. ./%s <video file> \n", argv[0]);
		return 1;
	}

	extractMeta(argv[1], &width, &height, &fcount, &fps);

	printf("%d %d %d %f\n", width, height, fcount, fps);

	//put meta in database.video_meta

	//get unique video_id
	//pass video_id as directory
	
	char *dir = "test";
	extractStills(argv[1], dir, fps);

	return 0;
}


void clear_buffer(char *buf) {
	int toClear = strlen(buf);
	memset(&buf[0], 0, sizeof(buf));
}

void readToBuf(FILE *pipe, char *buf, char *cmmd, char const *vid) {
	strcat(cmmd, vid);
	if ((pipe = popen(cmmd, "r")) == NULL) {
		perror("popen");
		exit(1);
	}

	fgets(buf, 80, pipe);

	pclose(pipe);
}


void extractMeta(char const *video_path, int *width, int *height, int *fcount, float *fps) {
	FILE *pipe_fp;
	char readbuf[80];
	char fcount_probe_cmmd[1024] = "ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of default=nokey=1:noprint_wrappers=1 ";
	char fps_probe_cmmd[1024] = "ffprobe -v error -select_streams v:0 -show_entries stream=avg_frame_rate -of default=noprint_wrappers=1:nokey=1 ";
	char y_probe_cmmd[1024] = "ffprobe -v error -of flat=s=_ -select_streams v:0 -show_entries stream=height ";
	char x_probe_cmmd[1024] = "ffprobe -v error -of flat=s=_ -select_streams v:0 -show_entries stream=width ";

	/*
	Extract frame count information from video.
	*/

	readToBuf(pipe_fp, readbuf, fcount_probe_cmmd, video_path);

	if(sscanf(readbuf, "%d", fcount) != 1) {
		// printf("Frame Count could not be read.\n");
	}
	printf("Frame Count: %d\n", *fcount);
	clear_buffer(readbuf);


	/*
	Extract fps information from video.
	*/
	readToBuf(pipe_fp, readbuf, fps_probe_cmmd, video_path);

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
	readToBuf(pipe_fp, readbuf, y_probe_cmmd, video_path);

	if(sscanf(readbuf, "streams_stream_0_height=%d", height) != 1) {
		// printf("FPScould not be read.\n");
	}
	printf("Height: %d\n", *height);
	clear_buffer(readbuf);

	/*
	Extract width information from video.
	*/
	readToBuf(pipe_fp, readbuf, x_probe_cmmd, video_path);
	
	if(sscanf(readbuf, "streams_stream_0_width=%d", width) != 1) {
		// printf("FPScould not be read.\n");
	}
	printf("Width: %d\n", *width);
	clear_buffer(readbuf);
}


void extractStills(char const *video_path, char *dir, float fps) {
	FILE *pipe_fp;

	struct stat st = {0};
	//check if directory exists already
	if(stat(dir, &st) == -1) {
		mkdir(dir, 0700);
		printf("Created directory %s\n", dir);
	} else {
		printf("Directory %s already exists\n", dir);
	}


	char extract_frames_cmmd[1024] = "ffmpeg -i ";//IMG_2461.MOV -vf fps=29.982466 testout/out\%d.png"

	//construct ffmpeg command 
	strcat(extract_frames_cmmd, video_path);

	strcat(extract_frames_cmmd, " -vf fps=");
 
	//convert fps (float) to string
	char str_fps[10];
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

