-include ../Makefile.rules

# -print-search-dirs
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=quad
SCRIPTS=quad.sh

.PHONY : all install clean

all:$(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	@echo "---------- Build ${EXECUTABLE} application"
ifdef CROSS
	-@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
else
	libtool --mode=link ${CC} -Wall `pkg-config --cflags --libs gstreamer-0.10` ${OBJECTS} -o $@
endif

.c.o:
	@echo "---------- Build $<"
	-@$(CC) $(CFLAGS) $< -o $@

install:
	@echo "---------- Install ${EXECUTABLE} application on ${INSTALL_DIR}"
	@$ mv ${EXECUTABLE} ${INSTALL_DIR}
	@$ cp ${SCRIPTS} ${INSTALL_DIR}
ifdef CROSS
	@echo "---------- Install ${EXECUTABLE} application on ${TARGET}"
	-@$ sudo scp ${EXECUTABLE} ${TARGET}
	-@$ sudo scp ${SCRIPTS} ${TARGET}
endif

clean:
	@echo "---------- Remove ${EXECUTABLE} application"
	@$ if [ -e ${EXECUTABLE} ]; then rm ${EXECUTABLE}; fi;
	@echo "---------- Remove object files"
	@$ rm -f *.o

