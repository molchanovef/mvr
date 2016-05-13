-include Makefile.rules

TOP=$(shell pwd)

SCRIPTS=$(wildcard scripts/*.sh)

.PHONY : all pre clean gst mosaic record upload mvr install

all: clean pre gst mosaic record upload mvr install

pre:
	@mkdir -p ${TOP}/${BINARES}
	@mkdir -p ${TOP}/${BINARES}/${USER}
	@mkdir -p ${TOP}/${BINARES}/${USER}/${ARCH}
	
clean:
#	@rm -f ${TOP}/${BINARES}/${USER}/${ARCH}/*
	@${MAKE} -C gst $@
	@${MAKE} -C mosaic $@
	@${MAKE} -C record $@
	@${MAKE} -C upload $@
	@${MAKE} -C mvr $@
	
gst:
	@${MAKE} -C $@

mosaic:
	@${MAKE} -C $@

upload:
	@${MAKE} -C $@

record:
	@${MAKE} -C $@

mvr:
	@${MAKE} -C $@

install:
	@${MAKE} -C gst $@
	@${MAKE} -C mosaic $@
	@${MAKE} -C record $@
	@${MAKE} -C upload $@
	@${MAKE} -C mvr $@
	@$ sudo scp ${SCRIPTS} ${TARGET}

