# make rule primaria con dummy target ‘all’--> non crea alcun file all ma fa un complete build
# che dipende dai target client e server scritti sotto

all: dev serv

# make rule per il client
dev: dev.o
	gcc -Wall dev.o ./device/*.c ./API/logger.c ./utils/costanti.h ./utils/connection.c -o dev

# make rule per il server
serv: serv.o
	gcc	-Wall serv.o ./API/logger.c ./utils/costanti.h ./utils/connection.c -o serv

# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *o dev serv	

run:
	bash run.sh

build:
	make
	make run
