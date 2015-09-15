SOURCE=server.c common.h logg.h
MYPROGRAM=server
MYINCLUDES=/usr/local/lib

MYLIBRARIES=glog
CC=g++





all: $(MYPROGRAM)



$(MYPROGRAM): $(SOURCE)

	$(CC) -I$(MYINCLUDES) $(SOURCE) -o$(MYPROGRAM) -l$(MYLIBRARIES)

clean:

	rm -f $(MYPROGRAM)

