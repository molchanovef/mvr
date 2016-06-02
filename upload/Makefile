-include ../Makefile.rules

# -print-search-dirs
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=upload
SCRIPTS=

LDFLAGS = -pthread

.PHONY : all build install clean install

all: $(SOURCES) $(EXECUTABLE) install
	
$(EXECUTABLE): $(OBJECTS) 
	@echo "---------- Build ${EXECUTABLE} application"
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	@echo "---------- Build objects: ${OBJECTS}"
	$(CC) $(CFLAGS) $< -o $@

install:
	@echo "---------- Install ${EXECUTABLE} application on ${INSTALL_DIR}"
	@$ cp ${EXECUTABLE} ${INSTALL_DIR}
#	-@$ cp ${SCRIPTS} ${INSTALL_DIR}
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

