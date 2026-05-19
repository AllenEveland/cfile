CC		:= clang
CFLAGS  := -Wall -Wextra -Wunused-variable -Wunused-function -Wshadow -Wpedantic -std=c17 -O3 -march=x86-64-v3 -mtune=generic -flto=thin
LDFLAGS	:= -fuse-ld=mold -Wl,--gc-sections -Wl,--icf=all -Wl,-O3 -Wl,--strip-all
FFLAGS	:= -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-stack-protector
LIBS	:= -lmagic
SRC		:= cfile.c
EXE		:= cfile
DIR		:= /usr/local/bin

all: cfile.c
	$(CC) $(CFLAGS) $(FFLAGS) $(LDFLAGS) $(SRC) -o $(EXE) $(LIBS)

clean:
	rm -rf $(EXE)

install:
	cp $(SRC) $(DIR)

.PHONY: all clean install
