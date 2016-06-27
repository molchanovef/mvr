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

.c.o:
	@echo "---------- Build $<"
	-@$(CC) $(CFLAGS) $< -o $@

install: $(EXECUTABLE)
	@echo "---------- Install ${EXECUTABLE} application on ${INSTALL_DIR}"
	@$ cp ${EXECUTABLE} ${INSTALL_DIR}
#	-@$ cp ${SCRIPTS} ${INSTALL_DIR}

scp: install
ifdef CROSS
	@echo "---------- Install ${EXECUTABLE} application on ${TARGET}"
	-@$ sudo scp ${EXECUTABLE} ${TARGET}
#	-@$ sudo scp ${SCRIPTS} ${TARGET}
endif

clean:
	@echo "---------- Remove ${EXECUTABLE} application"
	@$ if [ -e ${EXECUTABLE} ]; then rm ${EXECUTABLE}; fi;
	@echo "---------- Remove object files"
	@$ rm -f *.o

