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

RUN mkdir /simplemail
RUN mkdir /simplemail/tests

# Build and configure dovecot
ENV USER root
COPY tests /simplemail/tests
COPY common-sources.mk /simplemail/common-sources.mk
RUN make -C /simplemail/tests dovecot-bin

# Execute tests
COPY . /simplemail/
RUN make -C /simplemail/tests
