-include Makefile.rules

TOP=$(shell pwd)

SCRIPTS=$(wildcard scripts/*.sh)

.PHONY : all pre clean mosaic record upload mvr install scripts

all: clean pre mosaic record upload mvr install

pre:
	@mkdir -p ${TOP}/${BINARES}
	@mkdir -p ${TOP}/${BINARES}/${USER}
	@mkdir -p ${TOP}/${BINARES}/${USER}/${ARCH}
	
clean:
	@rm -f ${TOP}/${BINARES}/${USER}/${ARCH}/*
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

scripts:
	@$ sudo scp ${SCRIPTS} ${TARGET}

