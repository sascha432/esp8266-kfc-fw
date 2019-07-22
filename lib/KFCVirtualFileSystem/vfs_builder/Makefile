
CCFLAGS = -g
C++ = c++ ${CCFLAGS}
CC = gcc ${CCFLAGS}

all: vfs_builder

vfs_builder: vfs_builder.o
	${C++} -o vfs_builder vfs_builder.o

tests: tests.o
	${C++} -o tests tests.o

vfs_builder.o: vfs_builder.cpp ../include/virtual_fs_struct.h
	${C++} -c vfs_builder.cpp

tests.o: tests.cpp ../include/virtual_fs_struct.h
	${C++} -c tests.cpp

clean:
	rm -f *.o vfs_builder tests
