all:
	cd myhttpd; make && cp server ../

clean:
	cd myhttpd; make clean && rm ../server
