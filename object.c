#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "object.h"
#include <assert.h>

void Object_destroy(void *self)
{
	Object *obj = self;

	if(obj) {
		if(obj->description) free(obj->description);
		free(obj);
	}
}


int Object_init(void *self)
{
	//says do nothing, really
	return 1;
}


void *Object_new(size_t size, Object proto, char *description)
{
	//set default function in case aren't already set
	if(!proto.init) proto.init = Object_init;
	if(!proto.destroy) proto.destroy = Object_destroy;

	//casting struct of one size, pointing it at different pointer
	//calloc(size_t count, size_t size)
	Object *el = calloc(1, size);
	*el = proto;

	//strdup duplicates a string and returns a pointer to it
	el->description = strdup(description);

	if(!el->init(el)) {
		//didn't init properly
		el->destroy(el);
		return NULL;
	} else {
		//made an object of any type
		return el;
	}
}