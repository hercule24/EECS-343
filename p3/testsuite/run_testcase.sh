#!/bin/bash

source config.test;

if [[ ! "$#" -eq 1 ]]; then
	echo -e "usage: $0 targz_file";
	exit 1;
fi;

TARGZ=$1;
TC_DIR=`pwd`;
TMP=`mktemp -d /tmp/cs343.tests.XXXXXX`;
OUTPUT=`mktemp -d /tmp/cs343.tests.XXXXXX`;
chmod go-rwx ${TMP} ${OUTPUT} || exit 1;

function cleanUp()
{
	rm -Rf ${TMP} ${OUTPUT};
}

echo "Testing ${TARGZ}";
echo;

# Untar sources
echo "UNTAR";
cd ${TMP} || { cleanUp; exit 1; }
tar xvfz ${TARGZ} || { cleanUp; exit 1; }

echo;

if [[ `ls *.c |wc -w` -eq 0 ]]; then
    echo "error: no source files (*.c) found";
	echo "Please follow the submission instructions carefully.";
	cleanUp;
    exit 1;
fi;

# Setup the environment
echo "SETUP";

for f in ${ORIG_FILES}; do
	cp -f ${TC_DIR}/$f .;
done	

if [ ! -f Makefile -a ! -f makefile ]; then
	echo "warning: Makefile is missing";
	echo;
fi;

# Compile the code
echo "COMPILE"
for f in ${PROGS}; do
	echo "compiling $f";
	FILES="";
	for src in ${SRCS}; do
		if [ -f ${src} ]; then
			FILES="$FILES ${src}";
		fi;
	done;
	${CC} ${CFLAGS} -D${f} -o $f ${FILES} >> ${OUTPUT}/gcc.output 2>&1;
	echo "----------" >> ${OUTPUT}/gcc.output;
	if [ ! -f ${f} ]; then
		${CC} ${CFLAGS} -D${f} -o $f ${FILES};
	fi;
done

# Make the competition file
# We'll see all compile errors/warnings in the compile step above
make all

WARNING=`grep -c warning ${OUTPUT}/gcc.output`
ERROR=`grep -c error ${OUTPUT}/gcc.output`

echo "${WARNING} warning(s) found while compiling"
echo "${ERROR} error(s) found while compiling"

if [ ${WARNING} -gt 0 -o ${ERROR} -gt 0 ]; then
	echo;
	echo "GCC OUTPUT";
	cat ${OUTPUT}/gcc.output;
fi;

if [ ${ERROR} -gt 0 ]; then
	echo "error: failed to create executable";
	cat ${OUTPUT}/gcc.output;
	cleanUp;
	exit 1;
fi;

echo;

# Testing
echo "TESTING SERVER";

for f in ${TRACES}; do
    
    ./${SERVER_BIN} > /dev/null &
    python ${TC_DIR}/${TESTING_PROG} ${HOST} ${PORT} ${TC_DIR}/$f
    
    kill $(jobs -p)
done

cleanUp;
