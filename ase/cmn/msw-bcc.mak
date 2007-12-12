NAME = asecmn

!ifndef MODE
MODE = release
!endif

CC = bcc32 
LD = ilink32
AR = tlib 

CFLAGS = -O2 -WM -WU -RT- -w -q -I..\..

OUT_DIR = ..\$(MODE)\lib
OUT_FILE_LIB = $(OUT_DIR)\$(NAME).lib

TMP_DIR = $(MODE)

OBJ_FILES_LIB = \
	$(TMP_DIR)\mem.obj \
	$(TMP_DIR)\str.obj \
	$(TMP_DIR)\misc.obj 

all: lib

lib: $(TMP_DIR) $(OUT_DIR) $(OUT_FILE_LIB) 

$(OUT_FILE_LIB): $(OBJ_FILES_LIB)
	$(AR) $(OUT_FILE_LIB) @&&!
+-$(**: = &^
+-)
!

$(TMP_DIR)\mem.obj: mem.c
	$(CC) $(CFLAGS) -o$@ -c mem.c

$(TMP_DIR)\str.obj: str.c
	$(CC) $(CFLAGS) -o$@ -c str.c

$(TMP_DIR)\misc.obj: misc.c
	$(CC) $(CFLAGS) -o$@ -c misc.c

$(OUT_DIR):
	-md $(OUT_DIR) 

$(TMP_DIR):
	-md $(TMP_DIR)

clean:
	-del $(OUT_FILE_LIB) 
	-del $(OBJ_FILES_LIB) 
