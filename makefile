#
# Makefile for the docker build environment and some other stuff
#

.PHONY: dockerimage
dockerimage:
	docker build -t sm .

.PHONY: login
login: | dockerimage
	docker run -ti sm bash

indep-include/subthreads-verifiers.h: indep-include/subthreads.h
	python gen-forward-verifiers.py >$@.new
	mv $@.new $@
