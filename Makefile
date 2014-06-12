CXX = g++					# compiler
CFLAGS = -g -Wall -Wno-unused-label
MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}}	# makefile name

OBJECTS0 = gbnReceiver.o			
EXEC0 = gbnReceiver

OBJECTS1 = gbnSender.o
EXEC1 = gbnSender

OBJECTS2 = srReceiver.o
EXEC2 = srReceiver

OBJECTS3 = srSender.o
EXEC3 = srSender

OBJECTS = ${OBJECTS1} ${OBJECTS2} ${OBJECTS3} ${OBJECTS0}
EXECS = ${EXEC1} ${EXEC2} ${EXEC3} ${EXEC0}	# all executables

#############################################################

all : ${EXECS}					# build all executables

${EXEC0} : ${OBJECTS0}				# link step for gbnReceiver executable
	${CXX} $^ ${CFLAGS} -o $@

${EXEC1} : ${OBJECTS1}				# link step for gbnSender executable
	${CXX} $^ ${CFLAGS} -o $@

${EXEC2} : ${OBJECTS2}				# link step for srReceiver executable
	${CXX} $^ ${CFLAGS} -o $@

${EXEC3} : ${OBJECTS3}				# link step for srSender executable
	${CXX} $^ ${CFLAGS} -o $@

#############################################################

${OBJECTS} : ${MAKEFILE_NAME}			

clean :						# remove files that can be regenerated
	rm -f *.o recvInfo channelInfo ${EXECS}
