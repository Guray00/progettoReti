# make rule primaria con dummy target ‘all’--> non crea alcun file all ma fa un complete build
# che dipende dai target client e server scritti sotto

all: dev server

# make rule per il client
dev: dev.o
	gcc -Wall client.o -o dev

# make rule per il server
server: server.o
	gcc	-Wall server.o -o server

# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *.o dev server	

run:
	bash run.sh

build:
	make
	make run