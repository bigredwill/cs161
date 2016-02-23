
CPPFLAGS += -I`pg_config --includedir`
OBJECTS = extract_images.o
LIBS = -L`pg_config --libdir` -lpq

prog: $(OBJECTS)
	cc $(CPPFLAGS) $(OBJECTS) $(LIBS) -o prog

clean:
	rm prog *.o 