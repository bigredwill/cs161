CC = g++

# define any compile-time flags
CFLAGS = -I`pg_config --includedir`

# define any directories containing header files other than /usr/include
INCLUDES = 

# define library paths in addition to /usr/lib
#   if I wanted to include libraries not in /usr/lib I'd specify
#   their path using -Lpath, something like:
LFLAGS = 

# define any libraries to link into executable:
#   if I want to link in libraries (libx.so or libx.a) I use the -llibname 
#   option, something like (this will link in libmylib.so and libm.so:
# -L/usr/local/pgsql/lib -lpq
#
LIBS = `pkg-config --cflags --libs opencv` -L`pg_config --libdir` -lpq 

# define the C source files
SRCS = determine_bounding_boxes.cpp bounding_box.c
A2SRCS = Simons_Will_assignment_2.c
A3SRCS = Simons_Will_assignment_3.cpp bounding_box.c
A4SRCS = draw_bounding_boxes.cpp bounding_box.c

# define the C object files 
#
# This uses Suffix Replacement within a macro:
#   $(name:string1=string2)
#         For each word in 'name' replace 'string1' with 'string2'
# Below we are replacing the suffix .c of all words in the macro SRCS
# with the .o suffix
#
OBJS = $(SRCS:.c=.o)
A3OBJS = $(A3SRCS:.c=.o)
A4OBJS = $(A4SRCS:.c=.o)

# define the executable file 
MAIN = prog

A3 = A3
A4 = A4


#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#

.PHONY: depend clean

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

$(A3): $(A3OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(A3) $(A3OBJS) $(LFLAGS) $(LIBS)

$(A4): $(A4OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(A4) $(A4OBJS) $(LFLAGS) $(LIBS)
# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(A3) $(A4)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
