CC = gcc
CFLAGS = -I.. -I../includes -I. -pthread

objects = connection.o daemon.o httpd_string.o \
memorypool.o reason_phrase.o response.o

server : main.o $(objects)
	$(CC) -o server $(CFLAGS) main.o $(objects)

main.o : ../httpd.h ../includes/configurations.h
$(objects) : ../httpd.h ../includes/internal.h
daemon.o connection.o response.o : connection.h
connection.o httpd_string.o : httpd_string.h
memorypool.o : memorypool.h
response.o : response.h
reason_phrase.o : reason_phrase.h

.PHONY : clean
clean :
	rm server main.o $(objects)
