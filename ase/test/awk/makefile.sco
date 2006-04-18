CC = cc
CFLAGS = -Xc -a ansi -O2 -I../../.. -D__STAND_ALONE
LDFLAGS = -L../../bas -L../../awk
LIBS = -lxpawk -lm

all: awk

awk: awk.o
	$(CC) -o awk awk.o $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o awk

.SUFFIXES: .c .o
.c.o:
	$(CC) -c $(CFLAGS) $< 

