TOP=../
-include ../Makefile.rules

# -print-search-dirs
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=upload
SCRIPTS=

LDFLAGS = -pthread

.PHONY : all install clean

all: $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	@echo "---------- Build ${EXECUTABLE} application"
	-@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "---------- Install ${EXECUTABLE} application on ${INSTALL_DIR}"
	@$ mv ${EXECUTABLE} ${INSTALL_DIR}
#	-@$ cp ${SCRIPTS} ${INSTALL_DIR}

.c.o:
	@echo "---------- Build $<"
	-@$(CC) $(CFLAGS) $< -o $@

scp: $(EXECUTABLE)
ifdef CROSS
	@echo "---------- Install ${EXECUTABLE} application on ${TARGET}"
	-@$ sudo scp ${INSTALL_DIR}/${EXECUTABLE} ${TARGET}
#	-@$ sudo scp ${INSTALL_DIR}/${SCRIPTS} ${TARGET}
endif

clean:
	@echo "---------- Remove ${EXECUTABLE} application"
	@$ if [ -e ${EXECUTABLE} ]; then rm ${EXECUTABLE}; fi;
	@echo "---------- Remove object files"
	@$ rm -f *.o

