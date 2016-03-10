/*

Will Simons
CS161
Assignment 3: Determine bounding boxes

bounding_box.c

Functions for modifying/creating/destroying BoundingBox structs ("objects")

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bounding_box.h"

BoundingBox *BoundingBox_new()
{
	BoundingBox *bb = (BoundingBox *) malloc(sizeof(BoundingBox));
	return bb;
};

void BoundingBox_destroy(BoundingBox *bb)
{
	if(bb) {
		free(bb);
	}
};

void BoundingBox_init(BoundingBox *bb, const char *name) {
	bb->x = 0;
	bb->y = 0;
	bb->width = 0;
	bb->height = 0;
	bb->min_width = 0;
	bb->min_height = 0;
	bb->max_width = 0;
	bb->min_width = 0;
	bb->name = (char *) name;
}

void BoundingBox_clear(BoundingBox *bb) {
	bb->x = 0;
	bb->y = 0;
	bb->width = 0;
	bb->height = 0;
	bb->min_width = 0;
	bb->min_height = 0;
	bb->max_width = 0;
	bb->min_width = 0;
}

void BoundingBox_print(BoundingBox *bb) {
	printf("Bounding Box, %s. w: %d h: %d x: %d y: %d\n", bb->name, bb->width, bb->height, bb->x, bb->y);
}