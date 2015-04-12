#
# A simple docker file for building and unit testing
#

# Our build requirements

FROM debian:jessie

# Add adtools repo
RUN echo deb http://dl.bintray.com/sba1/adtools-deb / >>/etc/apt/sources.list

RUN apt-get update
RUN apt-get install -y --no-install-recommends \
	lhasa \
	libcunit1-dev \
	valgrind \
	wget

RUN apt-get install -y --force-yes --no-install-recommends \
        adtools-binutils \
        adtools-sdk \
        adtools-gcc

RUN apt-get install -y --no-install-recommends \
	libglib2.0-dev \
	bzip2 \
	pkg-config \
	make \
	libssl-dev \
	libgtk2.0-dev \
	gcc

RUN useradd simplemail -m

WORKDIR /home/simplemail
RUN mkdir -p /home/simplemail/simplemail/tests

# Build and configure dovecot
COPY tests/makefile /home/simplemail/simplemail/tests/makefile
COPY common-sources.mk /home/simplemail/simplemail/common-sources.mk
RUN chown -R simplemail /home/simplemail/simplemail
USER simplemail
ENV USER simplemail
RUN make -C /home/simplemail/simplemail/tests dovecot-bin

# Execute tests
COPY . /home/simplemail/simplemail/
USER root
RUN chown -R simplemail /home/simplemail/simplemail
USER simplemail
WORKDIR /home/simplemail/simplemail
RUN make -C /home/simplemail/simplemail/tests
