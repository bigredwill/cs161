#ifndef _object_h
#define _object_h

typedef enum {
	NORTH, SOUTH, EAST, WEST
} Direction;

typedef struct {
	int (*init)(void *self);
	void (*destroy)(void *self);
	int (*attack)(void *self, int damage);
} Object;

int Object_init(void *self);
void Object_destroy(void *self);
void *Object_new(size_t size, Object proto, char *description);

#define NEW(T, N) Object_new(sizeof(T), T##Proto, N)
#define _(N) proto.N

#endif