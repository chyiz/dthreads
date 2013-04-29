LIBS = -lm -ldl
CFLAGS = -m32 -Wall -g -O3 -Wl,-Tdata,0x80a8000
CV_DTHREAD_LIBS = $(LIBS) -rdynamic ../src/libdthreads32_cv.so -lksnap -lrt
PTHREAD_LIBS = $(LIBS) -lpthread

all:
	gcc $(CFLAGS) bug-atom.c      -o bug-atom $(PTHREAD_LIBS)
	gcc $(CFLAGS) bug-atom.c      -o bug-atom-det $(CV_DTHREAD_LIBS)
	gcc $(CFLAGS) bug-deadlock1.c -o bug-deadlock1 $(PTHREAD_LIBS)
	gcc $(CFLAGS) bug-deadlock1.c -o bug-deadlock1-det $(CV_DTHREAD_LIBS)
	gcc $(CFLAGS) bug-deadlock.c  -o bug-deadlock $(PTHREAD_LIBS)
	gcc $(CFLAGS) bug-deadlock.c  -o bug-deadlock-det $(CV_DTHREAD_LIBS)
	gcc $(CFLAGS) bug-lockuse.c   -o bug-lockuse $(PTHREAD_LIBS)
	gcc $(CFLAGS) bug-lockuse.c   -o bug-lockuse-det $(CV_DTHREAD_LIBS)
	gcc $(CFLAGS) bug-order.c     -o bug-order $(PTHREAD_LIBS)
	gcc $(CFLAGS) bug-order.c     -o bug-order-det $(CV_DTHREAD_LIBS)
	gcc $(CFLAGS) bug-race.c      -o bug-race $(PTHREAD_LIBS)
	gcc $(CFLAGS) bug-race.c      -o bug-race-det $(CV_DTHREAD_LIBS)
	g++ $(CFLAGS) -o lock-dthread lockowner.c $(CV_DTHREAD_LIBS) 
	g++ $(CFLAGS) -o lock-pthread lockowner.c $(PTHREAD_LIBS) 

