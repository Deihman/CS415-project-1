uspsv1 : uspsv1.o p1fxns.o
	cc -o uspsv1 uspsv1.o p1fxns.o

uspsv2 : uspsv2.o p1fxns.o
	cc -o uspsv2 uspsv2.o p1fxns.o

uspsv3 : uspsv3.o p1fxns.o
	cc -o uspsv3 uspsv3.o p1fxns.o

uspsv4 : uspsv4.o p1fxns.o
	cc -o uspsv4 uspsv4.o p1fxns.o

uspsv1.o : uspsv1.c p1fxns.h 
	cc -c uspsv1.c

uspsv2.o : uspsv2.c p1fxns.h
	cc -c uspsv2.c

uspsv3.o : uspsv3.c p1fxns.h
	cc -c uspsv3.c

uspsv4.o : uspsv4.c p1fxns.h
	cc -c uspsv4.c

p1fxns.o : p1fxns.c p1fxns.h
	cc -c p1fxns.c