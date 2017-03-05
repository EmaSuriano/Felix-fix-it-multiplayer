all: Torneo
all: Partida
all: Cliente
clean: cleanMenu cleanPartida cleanTorneo cleanCliente

Torneo:torneo.o
	gcc torneo.o -lpthread -o Torneo -lSDL -lSDL_image -lSDL_ttf -lX11
torneo.o:torneo.c
	gcc -c torneo.c -lpthread -o torneo.o -std=gnu99

Partida:partida.o
	gcc partida.o -lpthread -o Partida
partida.o:partida.c
	gcc -c partida.c -lpthread -o partida.o -std=gnu99
	
Cliente:cliente.o menuPrincipal.o
	g++ cliente.o  menuPrincipal.o -lpthread -o Cliente -lSDL -lSDL_image -lSDL_ttf -lX11
cliente.o:cliente.c menuPrincipal.h
	g++ -c cliente.c -lpthread -o cliente.o 

menuPrincipal.o: menuPrincipal.c menuPrincipal.h
	g++ -o menuPrincipal.o  -c menuPrincipal.c -lpthread -lSDL -lSDL_image -lSDL_ttf

cleanObj:
	rm *.o
cleanMenu:
	rm menuPrincipal.o
cleanPartida:
	rm Partida partida.o
cleanTorneo:
	rm Torneo torneo.o
cleanCliente:
	rm Cliente cliente.o
