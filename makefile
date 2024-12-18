# Huan Nguyen
# huannguy@pdx.edu

CC = gcc
WERROR = 
WERROR = -Werror
CFLAGS = -Wall -Wextra -Wshadow -Wunreachable-code -Wredundant-decls \
	-Wmissing-declarations -Wold-style-definition -Wmissing-prototypes \
	-Wdeclaration-after-statement -Wno-return-local-addr -Wunsafe-loop-optimizations -Wuninitialized $(WERROR) 

PROG1 = viktar

TARGETS = $(PROG1) 

#INCLUDES = viktar.h

all: $(TARGETS)

# $@ is the target filename
# $* is the basename of the target filename
# $^ is the names of all the prerequisites
# $< is the name of the first dependency
# $? is the names of all prerequisites that are newer than the target

$(PROG1): $(PROG1).o  
	$(CC) $(CFLAGS) $^ -o $@ -lmd

$(PROG1).o: $(PROG1).c $(PROG1).h
	$(CC) $(CFLAGS) -c $< -g

# Non-build commands
############################################################################## 

.PHONY: clean cls ci git get zip tar

clean cls:
	rm -f *.o $(TARGETS) *.lst *.zip *.tar.gz
	rm -f *~ \#*

ci:
	if [ ! -d RCS ]; then mkdir RCS; fi
	ci -t-none -l -m "Lab2 backup" [Mm]akefile *.[ch]
	
git get:
	if [ ! -d .git ]; then git init; fi
	git add *.[ch] ?akefile
	git commit -m "Lab2 backup"

# .zip and .tar files
# ##########################################################################
ASSIGNMENT = Lab3


TAR_FILE = huannguy_$(ASSIGNMENT).tar.gz

tar:
	rm -f $(TAR_FILE)
	tar cvzf $(TAR_FILE) *.c [Mm]akefile

