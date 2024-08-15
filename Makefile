#
# Students' Makefile for the Malloc Lab
#
TEAM = bovik
VERSION = 1
HANDINDIR = /afs/cs.cmu.edu/academic/class/15213-f01/malloclab/handin

CC = gcc
CFLAGS = -Wall -O2 -m32
# CFLAGS = -Wall -Og -m32 -g

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

handin:
	cp mm.c $(HANDINDIR)/$(TEAM)-$(VERSION)-mm.c

clean:
	rm -f *~ *.o mdriver



OBJS2 = mdriver.o mm2.o memlib.o fsecs.o fcyc.o clock.o ftimer.o
mdriver2: $(OBJS2)
	$(CC) $(CFLAGS) -o mdriver2 $(OBJS2)
mm2.o: mm2.c mm.h memlib.h


OBJS3 = mdriver.o mm3.o rbtree.o memlib.o fsecs.o fcyc.o clock.o ftimer.o
mdriver3: $(OBJS3)
	$(CC) $(CFLAGS) -o mdriver3 $(OBJS3)
mm3.o: mm3.c mm.h memlib.h rbtree.h
rbtree.o: rbtree.c rbtree.h