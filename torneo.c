#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include "X11/Xlib.h"
#include <SDL/SDL_ttf.h>

#define TAMBUF 512
#define MAXQ 10
#define WinX 1012
#define WinY 526
#define REGISTROS 5

//estados de los jugadores
#define JUGANDO 0
#define DISPONIBLE 1
#define ESPERANDO 2
#define ABANDONO 3
#define PRIORIDAD 4

union semun{
	int val;
	struct semid_ds *buf;
	ushort * array;
}arg;

struct persona{
	struct in_addr ipPersona;
	int socket;
	int cantVida;
	int puntos; 
	int inmunidad;
	int id; //Me permite acceder a la matriz
	int puesto; //Me permite acceder al vector, varia segun los puntos
	char nombre[10];
	char estado;
	int sala;
	float promedio;
	int contrincanteID;
	int berserker;
	int racha;
};

struct config{ // config del archivo
	uint puerto;
	int duracion;
	int inmunidad;
};

struct partida{
	int IDpartida;
	int IDpersonaA;
	int IDpersonaB;
	int puntosA;
	int puntosB;
	int sala;
};

void * eventHandler(void * args);
void * verificarPartidas(void * args);
void * mostrarGrafica(void * args);
void * calcularTiempo(void * args);
void * escucharPersonas(void * args);
void * readShMem(void * args);
void * ordenarRanking(void * args);
void rellenarCadena(char * argumento);
SDL_Color newColor(int r, int g, int b);
void inicializarRectangulo(SDL_Rect * rect, int x,int y,int w,int h);
void cerrarServer();
void mostrarHistorialPartidas();
void nuevaPersona(struct persona *persona, struct in_addr *ipPersona, int socket);
void mostrarDatosPersona(struct persona *pers);
int obtenerSemaforo(key_t clave,int cantsem);
int setiarUnSemaforo(int IdSemaforo,int numSem,int valor);
void pedirSemaforo(int IdSemaforo,int numSem, int cantOp) ;
void devolverSemaforo(int IdSemaforo,int numSem,int cantOp);
void eliminarSemaforo(int IdSemaforo);
int setiarConfig(struct config *dato);
void mostrarRanking();
void increaseColor(SDL_Color *color);
void generarMarco(int x, int y, int tamX, int tamY, int division, SDL_Color *color, SDL_Surface *screen);
void numeroATiempo(char * tiempo, int num);
void escribirEnPantalla(char * content, int x, int y, TTF_Font *font, SDL_Color color, SDL_Surface *screen);
Uint32 colorToMap(SDL_Color color, SDL_Surface *screen);
int cargarImagen(SDL_Surface ** imagen, const char * ruta);
void estadoPalabra(int est, char* res);

int socketServidor; //socket server

int **partidas; // Por ahora guarda el estado de las partidas de cada jugador, 1 corriendo, 0 nada
struct persona *jugadores; //vector de jugadores dinamico guarda los puntos totales
int cantJ = 0; //Cantidad de elementos del vector y la matriz
int abandonos = 0;
int cantH = 0; //Cantidad de partidas jugadas
struct partida *historialPartidas;
int *puesto; //Guarda los puestos segun los IDS, este se va ordenando

int idShMem; //ID memoria compartida
int idSemReadShMem; //ID Semaforo para leer en la memoria compartida
int idSemWriteShMem; //ID Semaforo para escribir en la memoria compartida
int idSemChild; //ID del semaforo que habilita al hijo a leer

int terminado = 0;
int salir = 0;
int tiempo;
int cantEnCurso = 0;

pthread_t thShMem;
pthread_t threadEscucha;
struct config configuracion;

int main(int argc, const char *argv[]) {
	XInitThreads();
	char buffer[TAMBUF];
	//Creo el thread que recibe nuevas conexiones
	pthread_attr_t attr; // thread attribute
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);  // atributo Thread detached
	pthread_create(&threadEscucha, &attr,&escucharPersonas, NULL);
	
	signal(SIGINT, cerrarServer); //asigno handler a la señal de interrupcion.


	pthread_t threadGrafica;
	pthread_create(&threadGrafica, &attr, &mostrarGrafica, NULL); // se Lanza el thread que controla la parte grafica del servidor

	//Inicializo las variables dinamicas
	partidas = (int**) calloc(1, sizeof(int));
	jugadores = (struct persona*) calloc(1, sizeof(struct persona)); 
	historialPartidas = (struct partida*) calloc(1, sizeof(struct partida));

	//Semaforos de la memoria compartida, es un mutex y 2 mas para leer/escribir
	key_t ShMemKey = ftok(".", 4000);
	idSemReadShMem = obtenerSemaforo(ShMemKey, 1);  // semaforo para leer de la memoria compartida
	setiarUnSemaforo(idSemReadShMem, 0, 0);
	ShMemKey = ftok(".", 3000);
	idSemWriteShMem = obtenerSemaforo(ShMemKey, 1); // semaforo para escrbir de la memoria compartida
	setiarUnSemaforo(idSemWriteShMem, 0, 1);
	ShMemKey = ftok(".", 2000);
	idSemChild = obtenerSemaforo(ShMemKey, 1);  // semaforo para evitar que una partida escriba cuando otra tiene que leer
	setiarUnSemaforo(idSemChild, 0, 1);

	//Memoria compartida
	struct persona *memoria;
	key_t keyShMem = ftok(".", 5000);
	idShMem = shmget(keyShMem,sizeof(struct persona) * 2,IPC_CREAT | 0660);  // Crea la memoria compartida
	pthread_create(&thShMem,NULL,&readShMem, (void *) &jugadores); //thread que lee constantemente de la ShMem
	int sala = 1;
	int flagTime = 1;
	pthread_t thPartida;
	struct partida enCurso;
	struct persona personaA; //Son las dos personas a las que les asignara una partida
	struct persona personaB;
	int flag = 0;
	while(!terminado && !salir) { //Mientras dure el torneo. X- salir, terminado- tiempo
		for (int i = -1; !terminado && !salir && i <= cantJ; i++){  // para Reasignar Partidas
			if(i > 0){ // i > 0 mas de un jugador.
				flag = 0;
				if(jugadores[i-1].estado != ABANDONO && jugadores[i-1].estado != JUGANDO && send(jugadores[i-1].socket, "ACK", TAMBUF, MSG_NOSIGNAL) < 0){  // Verifca que un jugador no se cuelgue. Mientras espera
					jugadores[i-1].estado = ABANDONO;
					abandonos++;
					printf("Se murio el jugador %d\n", i-1);
					close(jugadores[i-1].socket);
					if(partidas[i-1][i-1] == 0)
						jugadores[i-1].promedio = -1;
				}
				switch(jugadores[i-1].estado){ // Define que accion realizar de acuerdo al estado del jugador
					case PRIORIDAD:  // cuando ya jugo contra todos
						for (int j = 0; j < cantJ; j++){
							if(i-1 != j && jugadores[j].estado == PRIORIDAD){
								//printf("Encontrado rivales: %d VS %d\n", i-1, j);
								jugadores[i-1].contrincanteID = j;
								jugadores[j].contrincanteID = i-1;
								jugadores[i-1].estado = ESPERANDO;
								if(jugadores[j].estado != JUGANDO)
									jugadores[j].estado = ESPERANDO;
								j = cantJ;
							}
						}
					break;
					case DISPONIBLE:  // busca un rival y verifica que no jugo contra todos.
						for (int j = 0; j < cantJ; j++){
							if(i-1 != j && partidas[i-1][j] == 0 && jugadores[j].estado != ABANDONO){
								printf("Encontrado rivales: %d VS %d\n", i-1, j);
								jugadores[i-1].contrincanteID = j;
								jugadores[j].contrincanteID = i-1;
								jugadores[i-1].estado = ESPERANDO;
								flag = 1;
								if(jugadores[j].estado != JUGANDO){
									jugadores[j].estado = ESPERANDO;
									j = cantJ;
								}
							}
							if(!flag)
								jugadores[i-1].estado = PRIORIDAD;
						}
					break;
					case ESPERANDO: // Encontro un rival e intenta inicializar la partida
						switch(jugadores[jugadores[i-1].contrincanteID].estado){
							case DISPONIBLE:
							case ESPERANDO:
							case PRIORIDAD:
								jugadores[jugadores[i-1].contrincanteID].estado = ESPERANDO;
								personaA = jugadores[i-1];
								personaB = jugadores[jugadores[i-1].contrincanteID];
								i = cantJ+1;
								printf("Asignando partida %d vs %d!\n", personaA.id, personaB.id);
							break;
							case ABANDONO:
							case JUGANDO:
								jugadores[i-1].estado = DISPONIBLE;
							break;
						}
					break;
				}
			}
			if(i == cantJ){  // si recorrio todo el vector reinicia la i.
				i = -1;
				sleep(1);
			}
		}
		if(!terminado && !salir){ 
			strcpy(buffer, "conectado");
			send(personaA.socket, buffer, TAMBUF, MSG_NOSIGNAL);  // envia conectado
			perror("send");
			send(personaB.socket, buffer, TAMBUF, MSG_NOSIGNAL);
			perror("send");
			printf("Sala abierta.\n");
			personaA.estado = JUGANDO;  // cambia el estado de los jugadores
			personaB.estado = JUGANDO;
			jugadores[personaA.id].estado = JUGANDO;
			jugadores[personaB.id].estado = JUGANDO;
			personaA.sala = sala;  // seta la sala
			personaB.sala = sala;
			jugadores[personaA.id].sala = sala;
			jugadores[personaB.id].sala = sala;
			pedirSemaforo(idSemChild, 0 , -1);   // le escribe a la partida los datos de entrada de los jugadores y la sala
			pedirSemaforo(idSemWriteShMem, 0 ,-1);  // la partida libera los semaforos cuando termina de leer.
			memoria = shmat(idShMem, NULL, 0);
			memoria[0] = personaA;
			memoria[1] = personaB;

			if((enCurso.IDpartida = fork())==0){ //creamos la sala 
				close(socketServidor);
				char* argum[4];
				char aux[4];
				char salaaux[2];
				sprintf(salaaux, "%d", sala);
				argum[0] = "./Partida";
				argum[1] = salaaux;
				sprintf(aux, "%d", configuracion.inmunidad);
				argum[2] = aux;
				argum[3] = NULL;
				if(execv(argum[0], argum) == -1)  // Lanzamos la partida con argumentos de sala y tiempo de inmunidad de la torta
					printf("Error: No se pudo abrir la partida\n");
				perror("execv");
				return 1;
			}else{ //el padre actualiza las partidas de ese jugador
				partidas[personaA.id][personaA.id]++; //Actualiza la cantidad de partidas
				partidas[personaB.id][personaB.id]++;
				partidas[personaA.id][personaB.id]++;
				partidas[personaB.id][personaA.id]++;
				enCurso.IDpersonaA = personaA.id;  // struct de la partida q se lanzo
				enCurso.IDpersonaB = personaB.id;
				enCurso.sala = sala;
				sala ++; 

				pthread_create(&thPartida,&attr,&verificarPartidas, (void *) &enCurso); // Thread que espera que la partida finalize
				if(cantJ>=2 && flagTime){  // Inicializa el tiempo del torneo. cuando hay al menos 2 jugadores.
					flagTime = 0;
					tiempo = configuracion.duracion *60;
					alarm(tiempo);
					signal(SIGALRM, cerrarServer); 
					pthread_t thTiempo;
					pthread_create(&thTiempo, NULL, &calcularTiempo, (void*)tiempo);
				}
			}
			//mostrarHistorialPartidas();// Muestro la matriz
		}
	}
	cerrarServer();
	//pthread_join(threadGrafica, NULL);
	//cerrarServer();
	return 0;
}

void * verificarPartidas(void * args){  // thread que se encarga de testiar si la partida termino
	cantEnCurso++;
	struct partida *punt = (struct partida*) args;
	struct partida p = *punt;
	int finalizar = 0;
	char buffer[TAMBUF];
	waitpid(p.IDpartida, NULL, 0);
	sleep(1);
	if(jugadores[p.IDpersonaA].sala > 0 || jugadores[p.IDpersonaB].sala > 0){  // en caso de que la partida finalizo inesperadamente.
		strcpy(buffer, "PARTIDAOFF");											// le avisa a los clientes y cierra lo conexion con los mismos
		if(jugadores[p.IDpersonaA].sala == p.sala){
			send(jugadores[p.IDpersonaA].socket, buffer, TAMBUF, MSG_NOSIGNAL);
			if(recv(jugadores[p.IDpersonaA].socket, buffer, TAMBUF, 0) <= 0)
				shutdown(jugadores[p.IDpersonaA].socket, SHUT_RDWR); // cierra el socket con ese cliente.
			//close(jugadores[p.IDpersonaA].socket);
			perror("shutdown");
			jugadores[p.IDpersonaA].estado = ABANDONO;
			abandonos++;
			if(partidas[p.IDpersonaA][p.IDpersonaA] == 1)
				jugadores[p.IDpersonaA].promedio = -1;
			printf("Se cerro la partida mal\n");
		}
		if(jugadores[p.IDpersonaB].sala == p.sala){
			send(jugadores[p.IDpersonaB].socket, buffer, TAMBUF, MSG_NOSIGNAL);
			if(recv(jugadores[p.IDpersonaB].socket, buffer, TAMBUF, 0) <= 0)
				shutdown(jugadores[p.IDpersonaB].socket, SHUT_RDWR); // cierra el socket con ese cliente.
			//close(jugadores[p.IDpersonaB].socket);
			perror("shutdown");
			jugadores[p.IDpersonaB].estado = ABANDONO;
			abandonos++;
			if(partidas[p.IDpersonaB][p.IDpersonaB] == 1)
				jugadores[p.IDpersonaB].promedio = -1;
			printf("Se cerro la partida mal\n");
		}
	}
	cantEnCurso--;
}

void * escucharPersonas(void * args){  // Thread que entabla las conexiones con los clientes. Listener
	//Configuracion del servidor
	struct sockaddr_in configuracionServer;
	setiarConfig(&configuracion);
	struct sockaddr_in configPersona;
	socklen_t sizeofa = sizeof(configuracionServer);
	int socketPersona;
	struct persona persona;
	socketServidor = socket(AF_INET, SOCK_STREAM, 0);
	perror("socket");
	configuracionServer.sin_family = AF_INET; //protocolo
	configuracionServer.sin_port = htons(configuracion.puerto); //configura el puerto
	configuracionServer.sin_addr.s_addr = htonl(INADDR_ANY); //que reciba de cualquier lado
	bind(socketServidor, (struct sockaddr *)&configuracionServer, sizeof(struct sockaddr));
	perror("bind");	
	listen(socketServidor, MAXQ); //definimos la maxima cola de clientes en espera
	perror("listen");
	if(errno == EADDRINUSE){
		printf("ERROR: El puerto ya esta en uso\n");
		exit(-1);
	}
	while(1){
		bzero(&configPersona, sizeof(configPersona));
		socketPersona = accept(socketServidor, (struct sockaddr *)(&configPersona), &sizeofa);
		perror("accept");
		nuevaPersona(&persona, &configPersona.sin_addr, socketPersona);
	}
}

void nuevaPersona(struct persona *persona, struct in_addr *ipPersona, int socket){  // Funcion que genera las nuevas personas y actualiza los vectores dinamicos
	char buffer[TAMBUF];
	persona->ipPersona= *ipPersona;
	persona->puntos = 0;
	persona->cantVida= 3;
	persona->inmunidad=0;
	persona->socket= socket;
	persona->id = cantJ;//A cada nueva persona le asgina su id (nº de jugador)
	persona->puesto = cantJ; //A cada persona le asigna el ultimo puesto cuando se une al torneo
	persona->estado = DISPONIBLE;
	persona->promedio = 0;
	persona->contrincanteID = -1;
	persona->berserker = 0;
	persona->racha = 0;
	bzero(buffer, TAMBUF); //rellena con ceros el resto de la estructura
	if(recv(socket, buffer, TAMBUF, 0) >= 0){  // Espera el nombre del jugador
		strcpy(persona->nombre,buffer);
		cantJ++;
		partidas = (int**) realloc(partidas, sizeof(int) * cantJ * cantJ); //reacomodo la matriz
		jugadores = (struct persona*) realloc(jugadores, sizeof(struct persona) * cantJ); //reacomodo el vector de jugadores
		puesto = (int*) realloc(puesto, sizeof(int) * cantJ); //Reacomodo el vecto de puestos
		jugadores[persona->id] = *persona;
		puesto[cantJ-1] = cantJ-1;
		partidas[cantJ-1] = (int*) calloc(cantJ, sizeof(int)); //Asigno memoria a la nueva fila, y la inicializo en 0
		for(int i = 0; i < cantJ; i++){ //Limpio la basura y reasigno cada fila
			partidas[i] = realloc(partidas[i], sizeof(int)*cantJ);
			partidas[i][cantJ-1] = 0;
		}// asigna columnas
		printf("Se conecto el jugador: %s\n",persona->nombre);
	}
}

void mostrarDatosPersona(struct persona *pers){   // muestra datos de las personas
	printf("--------------------------\n");
	printf("ID: %d\n", pers->id);
	printf("NOMBRE: %s\n", pers->nombre);
	printf("IP: %s\n", inet_ntoa(pers->ipPersona));
	printf("SOCKET: %d\n", pers->socket);
	printf("VIDAS: %d\n", pers->cantVida);
	printf("PUNTOS: %d\n", pers->puntos);
	printf("PUESTO: %d\n", pers->puesto+1);
	printf("PROMEDIO: %f\n", pers->promedio);
	printf("ESTADO: %d\n", pers->estado);
	printf("--------------------------\n");
}

void * ordenarRanking(void * args){  // Ordena los putanjes de los jugadores y les manda el puesto a cada uno. Ordena vector de puestos.
	int j;
	int aux;
	char buffer[TAMBUF];
	int rank = 1;
	for (int i = 1; i < cantJ; i++) {
		aux = puesto[i];
		//if(jugadores[aux].estado == ABANDONO)
		//	jugadores[aux].promedio = 0;
		j = i - 1;
		while (j >= 0 && jugadores[aux].promedio > jugadores[puesto[j]].promedio) {
			jugadores[aux].puesto--;
			jugadores[puesto[j]].puesto++;
			puesto[j+1] = puesto[j];
			j--;
		}
		puesto[j+1] = aux;
	}
	// METODO INSERCION

	for (int i = 0; i < cantJ; ++i){
		if(i > 0 && jugadores[puesto[i-1]].promedio != jugadores[puesto[i]].promedio)
				rank++;
		if(jugadores[puesto[i]].estado != ABANDONO){
			sprintf(buffer, "1 45 %d", rank);
			rellenarCadena(buffer);
			send(jugadores[puesto[i]].socket, buffer, TAMBUF, MSG_NOSIGNAL);
		}
	} // MANDA A TODOS EL RANKING

}

void rellenarCadena(char * argumento){  // funcion para rellenar la cadena del buffer
	int resto = 20 - strlen(argumento);
	int i;
	if(resto>1){
		strcat(argumento," ");
		for(i = 0; i < resto-2; i++)
			strcat(argumento,"-");
		strcat(argumento,"\0");
	}
}

void mostrarRanking(){
	printf("--------------RANKING--------------\n");
	for(int i = 0; i < cantJ;i++){
		printf("PUESTO Nº%d\t", i+1);
		printf("ID: %d\t",puesto[i]);
		printf("Nombre: %s\t",jugadores[puesto[i]].nombre);
		printf("promedio: %f\n",jugadores[puesto[i]].promedio);
	}
	printf("-----------------------------------\n");
}

void * readShMem(void * args){ //Lee constantemente de la memoria, usa alternancia estricta
	struct persona *lectura;
	struct persona pers1; 
	struct persona pers2;//Variables auxliares donde se guarda la info, asi no se limita el uso de la memoria compartida
	pthread_t thOrdenar;
	while(!terminado){ //Hasta que no se agote el tiempo
		pedirSemaforo(idSemReadShMem, 0, -1); //Pide para leer
		if(!terminado){
			lectura = shmat(idShMem, NULL, 0); //Lee
			pers1 = lectura[0];
			pers2 = lectura[1];
			devolverSemaforo(idSemWriteShMem, 0 , 1); //Devuelve para escribir
			// Lee la memoria compartida
			//EN CADA EVENTO
			if(jugadores[pers1.id].estado != ABANDONO){
				pers1.promedio = ((float)jugadores[pers1.id].puntos + pers1.puntos) / partidas[pers1.id][pers1.id];
				jugadores[pers1.id].promedio = pers1.promedio;
			}

			if(jugadores[pers2.id].estado != ABANDONO){
				pers2.promedio = ((float)jugadores[pers2.id].puntos + pers2.puntos) / partidas[pers2.id][pers2.id];
				jugadores[pers2.id].promedio = pers2.promedio;
			}
			//Actualiza el promedio de los 2 jugadores

			if(pers1.estado == ABANDONO && jugadores[pers1.id].estado != ABANDONO){
				jugadores[pers1.id].estado = ABANDONO;
				abandonos++;
				if(partidas[pers1.id][pers1.id] == 1)
					jugadores[pers1.id].promedio = -1;
			} // actualiza el estado del jugador 1

			if(pers2.estado == ABANDONO && jugadores[pers2.id].estado != ABANDONO){
				jugadores[pers2.id].estado = ABANDONO;
				abandonos++;
				if(partidas[pers2.id][pers2.id] == 1)
					jugadores[pers2.id].promedio = -1;
			} // actualiza el estado del jugador 1

			//CUANDO TERMINO LA PARTIDA
			if(pers1.sala == 0 && pers2.sala == 0){
				jugadores[pers1.id].sala = 0;
				jugadores[pers2.id].sala = 0;
				if(pers1.estado == ABANDONO)
					shutdown(jugadores[pers1.id].socket, SHUT_RDWR); 
				if(pers1.estado != ABANDONO){
					jugadores[pers1.id].puntos += pers1.puntos;
					jugadores[pers1.id].estado = DISPONIBLE;
				}
				if(pers2.estado == ABANDONO)
					shutdown(jugadores[pers2.id].socket, SHUT_RDWR);
				if(pers2.estado != ABANDONO){
					jugadores[pers2.id].puntos += pers2.puntos;
					jugadores[pers2.id].estado = DISPONIBLE;
				} // caso de abandono cierra las conexiones, de lo contrario le actualiza los puntos y les cambia el estado a disponible.

				cantH++; // partidas jugadas
				historialPartidas = (struct partida*) realloc(historialPartidas, sizeof(struct partida) * cantH);  // agregamos una partida mas al historial de partidas.
				historialPartidas[cantH-1].IDpersonaA = pers1.id;
				historialPartidas[cantH-1].IDpersonaB = pers2.id;
				historialPartidas[cantH-1].puntosA = pers1.puntos;
				historialPartidas[cantH-1].puntosB = pers2.puntos;
			}
			//ordenarRanking();
			pthread_attr_t attr; // thread attribute
		 	pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_create(&thOrdenar,&attr,&ordenarRanking, NULL); // por cada lectura actualiza el ranking, por cada evento.
		}
	}
}

void mostrarHistorialPartidas(){  
	puts("-------HISTORIAL DE PARTIDAS--------");
	for (int i = 0; i < cantH; i++){
		puts("========================================");
		printf("SALA: %d\n", i);
		printf("Jugador %d vs. Jugador %d\n", historialPartidas[i].IDpersonaA, historialPartidas[i].IDpersonaB);
		printf("puntos: %d\tpuntos:%d\n", historialPartidas[i].puntosA, historialPartidas[i].puntosB);
		puts("========================================");
	}
	puts("------------------------------------");
}

void cerrarServer(){
	close(socketServidor);
	pthread_cancel(threadEscucha);
	perror("close");

	while(cantEnCurso) // Si hay partidad en curso espera hasta que terminen todas las partidas
		sleep(1);
	char buffer[TAMBUF];
	for(int i = 0; i < cantJ; i++){  //Le manda FIN a todos los jugadores conectados para que terminen.
		if(jugadores[i].estado != ABANDONO){
			sprintf(buffer, "FIN");	
			send(jugadores[i].socket, buffer, TAMBUF, MSG_NOSIGNAL);
			if(recv(jugadores[i].socket, buffer, TAMBUF, 0) <= 0)
			//usleep(500000);
				shutdown(jugadores[i].socket, SHUT_RDWR);
			//close(jugadores[i].socket);
			//printf("Se cerro el socket %d\n", i);
		}
	}
	terminado = 1;  // para que cierre los threads de memoria
	devolverSemaforo(idSemReadShMem, 0 ,1);
	pthread_join(thShMem, NULL);  // espera que termine el thread de la memoria compartida.
	eliminarSemaforo(idSemChild);
	eliminarSemaforo(idSemReadShMem);// elimina semaforos.
	eliminarSemaforo(idSemWriteShMem);
	shmctl(idShMem, IPC_RMID, 0); // libera memoria compartida
	printf("Se desconecto el socket server\n");
	exit(0);
	//kill(getpid(), SIGTERM);
}

void * calcularTiempo(void * args){  // thread que calcula el tiempo del torneo
	tiempo = (int) args;
	while(tiempo > 0){
		sleep(1);
		tiempo--;
	}
}

int setiarConfig(struct config *dato){ // Levanta el archivo de configuracion con sus recpectivos datos.
	FILE *fp;
	int error = 0;
	dato->puerto = 0;
	dato->duracion = -1;
	dato->inmunidad = -1;
	char linea[100];
	char palabra[10];
	int num;
	fp = fopen("configServer.cfg", "rt");
	if (fp == NULL){
		printf("No se encontro el archivo de configuracion del Servidor.\n");
		error++;
	}
	else{
	 	while(!feof(fp)){
	 		num = -1;
	 		fgets(linea,100,fp);
	 		sscanf(linea,"%s %d",palabra, &num);
	 		if(strcmp(palabra, "PORT:")==0 && num != -1)
	 			dato->puerto = num;
	 		if(strcmp(palabra, "Duracion:")==0 && num != -1)
	 			dato->duracion = num;
	 		if(strcmp(palabra, "Inmunidad:")==0 && num != -1)
	 			dato->inmunidad = num;
	 	}
 	}
 	if(dato->inmunidad < 0){
 		printf("No se encontro el parametro de la inmunidad en el archivo, o era invalido. Se cargo el valor por defecto 5.\n");
 		dato->inmunidad = 5;
 		error++;
 	}
 	if(dato->puerto == 0){
 		printf("No se encontro el parametro del puerto en el archivo, o era invalido. Se cargo el valor por defecto 8080.\n");
 		dato->puerto = 8080;
 		error++;
 	}
 	if(dato->duracion <= 0){
 		printf("No se encontro el parametro de la duracion en el archivo, o era invalido. Se cargo el valor por defecto 10.\n");
 		dato->duracion = 10;
 		error++;
 	}
 	if(error){
 		printf("Se volvio a generar el archivo con valores por defecto. Si no esta de acuerdo con estos valores, por favor cierre el servidor, modifique el archivo y ejecute el servidor nuevamente.\n");
 		fp = fopen("configServer.cfg", "wt");
 		sprintf(linea, "PORT: %d\n", dato->puerto);
 		fputs(linea, fp);
 		sprintf(linea, "Duracion: %d\n", dato->duracion);
 		fputs(linea, fp);
 		sprintf(linea, "Inmunidad: %d", dato->inmunidad);
 		fputs(linea, fp);
 	}
 	if(fp != NULL)
 		fclose (fp);
	return 0;
}

void pedirSemaforo(int IdSemaforo,int numSem, int cantOp) { //P(X)
	struct sembuf OpSem; 
	OpSem.sem_num = numSem; 
	OpSem.sem_op = cantOp; 
	OpSem.sem_flg = 0; 
	semop(IdSemaforo, &OpSem,1 ); 
} 

void devolverSemaforo(int IdSemaforo,int numSem,int cantOp) { //V(X)
	struct sembuf OpSem;
	OpSem.sem_num = numSem; 
	OpSem.sem_op = cantOp; 
	OpSem.sem_flg = 0; 
	semop(IdSemaforo, &OpSem, 1); 
}

int obtenerSemaforo(key_t clave,int cantsem){ 
	int IdSemaforo; 
	union semun CtlSem; 
	IdSemaforo = semget(clave, cantsem, IPC_CREAT | 0660); 
	return IdSemaforo; 
}

int setiarUnSemaforo(int IdSemaforo,int numSem,int valor){
	union semun CtlSem; 
	CtlSem.val = valor; 
	semctl(IdSemaforo, numSem, SETVAL, CtlSem); 
	return numSem; 
}

void eliminarSemaforo(int IdSemaforo) { 
	semctl(IdSemaforo, 0, IPC_RMID); 
} 

SDL_Color newColor(int r, int g, int b){
	SDL_Color color;
	color.r=r;
	color.g=g;
	color.b=b;
	return color;
}

void inicializarRectangulo(SDL_Rect * rect, int x,int y,int w,int h){// setea valores del rectangulo
	rect->x= x;
	rect->y= y;
	rect->w= w;
	rect->h= h;
}

void * mostrarGrafica(void * args){
	SDL_Surface* screen;
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		exit(-1);
	atexit(SDL_Quit); //si ocurrio un exit(), lo que hace esto es que libera todos los recursos SDL
	screen = SDL_SetVideoMode(WinX, WinY, 16, SDL_HWSURFACE);
	if (screen == NULL)
		exit(-1); 
	pthread_t threadControlador;
	pthread_create(&threadControlador, NULL, &eventHandler, NULL);
	char aux[50];
	SDL_Rect rectAux;
	SDL_Rect rectTitle;
	SDL_Surface * plantilla;
	TTF_Font *fontBig;
	TTF_Font *fontMedium;
	TTF_Font *fontSmall;
	TTF_Font *fontVerySmall;
	int activos;
	char palabra[15];
	if(TTF_Init()!=0) {
		printf("Error al iniciar TTF: %s\n", TTF_GetError());
		exit (-1);
    }
	if((TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 20)) == NULL) {
		printf("Error al abrir fuente: %s\n", TTF_GetError());
		exit (-1);
	}
	cargarImagen(&plantilla, "./ImagenesSDL/spritesFelix.png");
	inicializarRectangulo(&rectTitle, 0, 497, 428, 163); 
	SDL_Color color1= newColor(255,0,0);
	SDL_Color color2= newColor(0,255,0);
	SDL_Color color3= newColor(0,0,255);

	// Carga fuentes de distinto tamaño
	fontBig = TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 40); 
	fontMedium = TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 35);
	fontSmall = TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 20);
	fontVerySmall = TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 17);
	while(!terminado){
		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

		inicializarRectangulo(&rectAux, 548, 18, 0, 0);
		SDL_BlitSurface(plantilla, &rectTitle, screen, &rectAux);

		generarMarco(20,20,466, 143,1, &color1, screen);

		numeroATiempo(aux, tiempo);
		escribirEnPantalla((char *)"TIME", 50, 40, fontBig, newColor(255,255,255), screen); // escribe en pantalla
		escribirEnPantalla(aux, 90, 95, fontBig,newColor(255,255,255), screen);

		escribirEnPantalla((char *)"PLAYERS", 280, 45, fontMedium, newColor(255,255,255), screen);
		activos = cantJ - abandonos;
		sprintf(aux, "%d", activos);
		escribirEnPantalla(aux, 390, 95, fontBig,newColor(255,255,255), screen);

		generarMarco(20,220,466, 503-220,2, &color2, screen);

		// TITULOS DE TABLA
		escribirEnPantalla((char *)"Top Players", 110, 172, fontMedium,newColor(255,255,255), screen);
		escribirEnPantalla((char *)"Name", 40, 240, fontSmall,newColor(255,255,255), screen);
		escribirEnPantalla((char *)"Average", 195, 240, fontSmall,newColor(255,255,255), screen);
		escribirEnPantalla((char *)"State", 350, 240, fontSmall,newColor(255,255,255), screen);

		// LLena la tabla de ranking con datos.
		for(int i=0; i<REGISTROS && i<cantJ;i++)
			if(jugadores[puesto[i]].promedio != -1){
				estadoPalabra(jugadores[puesto[i]].estado, palabra);
				sprintf(aux, "%-10s %-11.2f %s", jugadores[puesto[i]].nombre, jugadores[puesto[i]].promedio, palabra);

				escribirEnPantalla(aux, 40, 240+(i+1)*40, fontSmall, newColor(255,255,255), screen);
			}

		generarMarco(526,220,466, 503-220,3, &color3, screen);

		// TITULOS DE TABLA
		escribirEnPantalla((char *)"Matches History", 570, 172, fontMedium,newColor(255,255,255), screen);
		escribirEnPantalla((char *)"ScoreP1", 544, 240, fontVerySmall,newColor(255,255,255), screen);
		escribirEnPantalla((char *)"Player 1", 658, 240, fontVerySmall,newColor(255,255,255), screen);
		escribirEnPantalla((char *)"Player 2", 774, 240, fontVerySmall,newColor(255,255,255), screen);
		escribirEnPantalla((char *)"ScoreP2", 886, 240, fontVerySmall,newColor(255,255,255), screen);

		int cont=0;
		// llena tabla del historial de partidas con datos.
		for(int i=cantH-1; cont<REGISTROS && i>=0;i--){
			sprintf(aux, "%-9d %-9s %-9s %d", 
				historialPartidas[i].puntosA,jugadores[historialPartidas[i].IDpersonaA].nombre, 
				jugadores[historialPartidas[i].IDpersonaB].nombre, historialPartidas[i].puntosB);

			escribirEnPantalla(aux, 544, 240+(cont+1)*40, fontVerySmall, newColor(255,255,255), screen);
			cont ++;
		}

		SDL_Flip(screen); // pasa todo a la pantalla
		SDL_Delay(200);
	}
	pthread_join(threadControlador, NULL);
}

void increaseColor(SDL_Color *color){ // funcion para hacer el cambio de colores de los marcos
	if(color->r == 255 && color->b==0 && color->g!=0)
		color->g -=51;
	else if(color->g == 255 && color->b==0 && color->r!=255)
		color->r +=51;
	else if(color->g == 255 && color->r==0 && color->b!=0)
		color->b -=51;
	else if(color->b==255 && color->r == 0 && color->g!=255)
		color->g +=51;
	else if(color->b==255 && color->g == 0 && color->r!=0)
		color->r -=51;
	else if(color->r == 255 && color->g==0 && color->b!=255)
		color->b +=51;
}

void generarMarco(int x, int y, int tamX, int tamY, int division, SDL_Color *color, SDL_Surface *screen){  // genera marcos para la pantalla
	SDL_Rect rectAux;
	inicializarRectangulo(&rectAux, x, y, tamX, tamY);
	SDL_FillRect(screen, &rectAux, SDL_MapRGB(screen->format, 255, 255, 255));
	inicializarRectangulo(&rectAux, x+5, y+5, tamX-10, tamY-10);
	SDL_FillRect(screen, &rectAux, SDL_MapRGB(screen->format, 0, 0, 0));
	inicializarRectangulo(&rectAux, x+8, y+8, tamX-16, tamY-16);
	increaseColor(color);
	Uint32 mapRGB= colorToMap(*color, screen);
	SDL_FillRect(screen, &rectAux, mapRGB);
	inicializarRectangulo(&rectAux, x+13, y+13, tamX-26, tamY-26);
	SDL_FillRect(screen, &rectAux, SDL_MapRGB(screen->format, 0, 0, 0));
	if(division==1){
		inicializarRectangulo(&rectAux, x+8+tamX/2, y+8, 5, tamY-16);
		SDL_FillRect(screen, &rectAux, mapRGB);
	}
	if(division==2){
		inicializarRectangulo(&rectAux, x+8, y+8+40, tamX-16, 5);
		SDL_FillRect(screen, &rectAux, mapRGB);
		inicializarRectangulo(&rectAux, x+tamX/3, y+8, 5, tamY-16);
		SDL_FillRect(screen, &rectAux, mapRGB);
		inicializarRectangulo(&rectAux, x+2*tamX/3, y+8, 5, tamY-16);
		SDL_FillRect(screen, &rectAux, mapRGB);
	}
	if(division==3){
		inicializarRectangulo(&rectAux, x+8, y+8+40, tamX-16, 5);
		SDL_FillRect(screen, &rectAux, mapRGB);
		inicializarRectangulo(&rectAux, x+tamX/4, y+8, 5, tamY-16);
		SDL_FillRect(screen, &rectAux, mapRGB);
		inicializarRectangulo(&rectAux, x+2*tamX/4, y+8, 5, tamY-16);
		SDL_FillRect(screen, &rectAux, mapRGB);
		inicializarRectangulo(&rectAux, x+3*tamX/4, y+8, 5, tamY-16);
		SDL_FillRect(screen, &rectAux, mapRGB);
	}
}

void numeroATiempo(char * tiempo, int num){ // cambia los segundoa  minutos
	char aux[10];
	int min=0;
	while(num>60){
		num-=60;
		min ++;
	}
	sprintf(tiempo, "%2d:%2d", min, num);
}

void escribirEnPantalla(char * content, int x, int y, TTF_Font *font, SDL_Color color, SDL_Surface *screen){  // para escribir en la pantalla dadas las cordenadas.
	SDL_Rect rectAux;
	SDL_Surface * texto;
	inicializarRectangulo(&rectAux, x, y, 0, 0);
	texto = TTF_RenderText_Blended(font, content, color);
	SDL_BlitSurface(texto,NULL,screen,&rectAux);
	SDL_FreeSurface(texto);
}

Uint32 colorToMap(SDL_Color color, SDL_Surface *screen){ // 
	return SDL_MapRGB(screen->format, color.r, color.g, color.b);
}

int cargarImagen(SDL_Surface ** imagen, const char * ruta){  // carga la imagen
	*imagen = IMG_Load(ruta);
	if (*imagen == NULL)
		return 1;
	return 0;
}

void estadoPalabra(int est, char* res){ // convierte el estado del jugador en una palabra
	switch(est){
		case JUGANDO:
			strcpy(res,"PLAYING");
		break;
		case DISPONIBLE:
			strcpy(res,"FREE");
		break;
		case PRIORIDAD:
		case ESPERANDO:
			strcpy(res,"WAITING");
		break;
		case ABANDONO:
			strcpy(res,"EXIT");
		break;
	}
}

void * eventHandler(void * args){  // si se apreta la x setea salir
	printf("eventHandler\n");
	SDL_Event evento;
	while (!terminado && !salir && SDL_WaitEvent(&evento))
		if(evento.type== SDL_QUIT)
			salir = 1;
	printf("Termina eventHandler\n");
}