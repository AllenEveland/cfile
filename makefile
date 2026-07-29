CC		:= gcc
CFLAGS  := -Wall -Wextra -Wshadow -Wpedantic -Wconversion -std=c17 -O2
LDFLAGS	:= -Wl,--gc-sections -Wl,--strip-debug
FFLAGS	:= -fno-unwind-tables -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-stack-protector
LIBS	:= -lmagic
SRC		:= cfile.c
EXE		:= cfile
DIR		:= /usr/local/bin

all: $(SRC)
	$(CC) $(CFLAGS) $(FFLAGS) $(LDFLAGS) $(SRC) -o $(EXE) $(LIBS)

clean:
	rm -rf $(EXE)

install:
	cp $(EXE) $(DIR)

.PHONY: all clean install
