#ifndef configurations_h
#define configurations_h

#define DEBUG 1

#ifdef __linux__
#define HAVE_SOCKADDR_IN_SIN_LEN 0
#else
#define HAVE_SOCKADDR_IN_SIN_LEN 1
#endif

#endif /* configurations_h */
