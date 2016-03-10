/*

Will Simons
CS161
Assignment 3: Determine bounding boxes

bounding_box.h

Bounding Box struct for easier manipulation of bounding box data.

*/

#ifndef _bounding_box_h
#define _bounding_box_h

typedef struct {
	int x;
	int y;
	int width;
	int height;
	int min_width;
	int max_width;
	int min_height;
	int max_height;
	char *name;
} BoundingBox;

BoundingBox *BoundingBox_new();
void BoundingBox_destroy(BoundingBox *bb);
void BoundingBox_init(BoundingBox *bb, const char *name);
void BoundingBox_clear(BoundingBox *bb);
void BoundingBox_print(BoundingBox *bb);

#endif