CXXFLAGS += -g -std=gnu++11 -DwxUSE_THREADS=1 $(shell wx-config --cxxflags base --debug)
LDLIBS = -g -lutil -pthread $(shell wx-config --libs --debug)


#-std=c++0x 
#wx-config -libs

.PHONY: all clean distclean

all: repeaterparrotserver

repeaterparrotserver: DVMegaWorkerThread.o DVAPWorkerThread.o BaseWorkerThread.o TxData.o RepeaterConnector.o DVAPWorkerThread.h BaseWorkerThread.h RepeaterConnector.h TxWorkerThread.o
	$(LINK.cpp) $^ $(LDLIBS) -o $@


clean:
	@- $(RM) *.o repeaterparrotserver

distclean: clean
