FROM ubuntu:16.04

RUN export DEBIAN_FRONTEND=noninteractive; apt-get -qq update; apt-get -y install cmake libxerces-c-dev \
libapr1-dev libaprutil1-dev wget libicu-dev liblucene++-dev liblucene++ bc \
icu-devtools bzip2 linux-headers-generic build-essential unzip software-properties-common curlftpfs \
python-software-properties autoconf libboost-all-dev libpodofo-dev libpodofo-utils doxygen libpqxx-dev \
fcgi++ libmagick++-dev libcurl4-openssl-dev postfix libdb5.3-dev libdb5.3++-dev libdb5.3++-stl-dev \
libeigen3-dev postgresql-client postgresql postgresql-contrib cimg-dev git lighttpd python3 python3-pip; \
wget http://apache.osuosl.org/activemq/activemq-cpp/3.9.4/activemq-cpp-library-3.9.4-src.tar.bz2 -O \
activemq.tar.bz2; tar xjf activemq.tar.bz2; cd activemq-cpp-library-3.9.4/; ./configure --enable-static \
--enable-shared --prefix=/usr/local; make && make install; cd ..; wget \
http://mirrors.gigenet.com/apache//uima//uimacpp-2.4.0/uimacpp-2.4.0-src.zip -O uimacpp.zip; unzip uimacpp.zip

# install oracle Jdk
RUN add-apt-repository ppa:webupd8team/java; echo debconf shared/accepted-oracle-license-v1-1 select true | \
debconf-set-selections; echo debconf shared/accepted-oracle-license-v1-1 seen true | debconf-set-selections; apt-get \
update -q; apt-get install -q -y oracle-java8-installer

# apply uima patches for CXX11-14
COPY ./patches/uimacpp-2.4.0/src/framework/uima/strtools.hpp /uimacpp-2.4.0/src/framework/uima/strtools.hpp
COPY ./patches/uimacpp-2.4.0/configure.ac /uimacpp-2.4.0/

# compile and install uima library
RUN cd uimacpp-2.4.0; autoconf -i; chmod +x configure; ./configure \
--with-activemq=/usr/local --with-icu=/usr \
--with-jdk="/usr/lib/jvm/java-8-oracle/include -I/usr/lib/jvm/java-8-oracle/include/linux" --enable-static \
--enable-shared --prefix=/usr/local; make && make install

# compile and install wt - the version provided by ubuntu is bugged!
RUN wget https://github.com/emweb/wt/archive/3.3.7.tar.gz -O wt-3.3.7.tar.gz; tar xzf wt-3.3.7.tar.gz; cd wt-3.3.7; mkdir build; cd build; \
cmake ..; make; make install

#google test
RUN git clone https://github.com/google/googletest.git; cd googletest; mkdir cmake-build-release; cd cmake-build-release; cmake ..; \
make; make install

# python packages
RUN pip3 install psycopg2

# perl packages
RUN cpan -i DBD::Pg

RUN git clone https://github.com/TextpressoDevelopers/libtpc.git; cd libtpc; mkdir cmake-build-release; \
cd cmake-build-release; cmake ..; make; make install

# clean
RUN rm -rf activemq*; rm -rf uimacpp*; rm -rf lucenepp.zip; rm -rf LucenePlusPlus-master; rm -rf wt-3.3.7*;

EXPOSE 80
