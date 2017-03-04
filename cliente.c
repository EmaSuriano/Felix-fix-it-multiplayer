#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <SDL/SDL.h>
#include <iostream>
#include <fstream>
#include <SDL/SDL_image.h>
#include "X11/Xlib.h"
#include <SDL/SDL_thread.h>
#include <SDL/SDL_ttf.h>
#include "menuPrincipal.h"

//DEFINE
#define TAMBUF 512
#define divSpriteX 56
#define divSpriteY 66
#define WinX 506
#define WinY 526
#define CAMBIO_NIVEL 10
#define CAMBIO_STAGE 30
#define RANKING 45
#define PLAYER1 1
#define PLAYER2 2
#define PAJARO 4
#define LADRILLO 8
#define RALPH 20
#define VENTANA 50
#define HAMMER 300
#define VIDA 80
#define TORTA 90
#define CARGAVENTANA 51
#define CARGAMARCO 52
#define CARGATECHO 53
#define NOMBREPLAYER 25

//ESTRUCTURAS
struct packageAnimator{
	SDL_Rect * rect;
	SDL_Rect * rectDestino;
	char orden[30];
};

struct player{
	int socket;
	int inmunidad;
	int cantidadVidas;
	int puntos;
	int berserker;
	int bloqueo;
	int ranking;
	struct config configuracion;
};

//GLOBALES SDL
TTF_Font *tipo_letra;

SDL_Surface * mapa;
SDL_Surface * plantilla;
SDL_Surface * pantalla;

SDL_Rect rectStage;

SDL_Rect rectFelix;
SDL_Rect rectFelixRed;

SDL_Rect rectDestinoFelix;
SDL_Rect rectDestinoFelixRed;

SDL_Rect rectRalph;
SDL_Rect rectDestinoRalph;

SDL_Rect rectPajaro[3];
SDL_Rect rectDestinoPajaro[3];

SDL_Rect rectLadrillo[3];
SDL_Rect rectDestinoLadrillo[3];

SDL_Rect rectPuntuacion[2];
SDL_Rect rectDestinoPuntuacion[2];

SDL_Rect rectTorta;
SDL_Rect rectDestinoTorta;

SDL_Rect rectGorro;

//GLOBALES
struct player player1;
struct player player2;
int singlePlayer=0;
int tiempoInmunidad;
int ventanas[15];
int techos[10];
int marcos[12];
int posX[]= {51, 83, 115, 147, 179, 211, 243, 275, 307, 339, 371, 403, 435};
int posY[]= {106, 214 , 248, 322, 356, 430};
int exitPPal=1;
int tiempoPatos;
int stage;
int level;
int tiempoLadrillos;
int numLadrillo=0;
float tiempoRalph;
int abandono=0;
int tiempoBerserker=0;
int tortaComida=0;

void * animatorWaiting(void * args);
void * exitListener(void * args);

int main(int argc, const char *argv[]) {
	XInitThreads();
	
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		return errorMessage("Error al inicializar el video");
	atexit(SDL_Quit); //si ocurrio un exit(), lo que hace esto es que libera todos los recursos SDL
	pantalla = SDL_SetVideoMode(WinX, WinY, 16, SDL_HWSURFACE);
	if (pantalla == NULL)
		return errorMessage("Error modo de video!");
	SDL_WM_SetCaption("Fix it Felix!", "Fix it Felix!");
	if(mostrarMenuPrincipal(pantalla))
		exit(0);
	
	struct sockaddr_in configSocket; //estructura de la configuracion del socket 
	cargarConfig(&player1.configuracion);
	player1.socket = socket(AF_INET, SOCK_STREAM, 0); //instanciamos el socket

	configSocket.sin_family = AF_INET;
	configSocket.sin_port = htons(player1.configuracion.port);
	configSocket.sin_addr.s_addr = inet_addr(player1.configuracion.IP);
	bzero(&(configSocket.sin_zero), 8);

	if(connect(player1.socket, (struct sockaddr *) &configSocket, sizeof(struct sockaddr))==-1){ //no se pudo conectar al servidor
		close(player1.socket);
		return errorMessage("El servidor no estaba disponible. Por favor intente mas tarde.\n");
	}
	enviar(player1.configuracion.nick);

	do{
		loadAssets();
		esperaRival(player1.socket);
		if(!abandono){
			inicializarVariablesInicio();

			pthread_t threadListener;
			pthread_t threadRender;
			pthread_create(&threadListener,NULL,&listener, (void *)NULL);
			pantallaCarga();
			pthread_create(&threadRender,NULL,&render, (void *)NULL);

			SDL_Event evento;
			while (exitPPal && SDL_WaitEvent(&evento))
				SDL_Delay(handleEvent(evento));

			pthread_join(threadRender, NULL);
			pthread_join(threadListener, NULL);
			enviar("FIN");
			if(esperaRespuestaTorneo()==0){
				printf("El servidor se ha caído!");
				abandono=2;
			}
			// printf("termina la vuelta\n");
		}
	}while(!abandono);
	close(player1.socket);
	printf("Posicion final en el torneo: %d\n", player1.ranking);
	printf("Juego Terminado!\n");
	exit(0);
}

int esperaRespuestaTorneo(){
	char buffer[TAMBUF];
	do{
		bzero(buffer, TAMBUF);
		recv(player1.socket, buffer, TAMBUF, 0); //receive bloqueante
		if(strcmp(buffer, "FIN")==0)
			strcpy(buffer, "TORNEOOFF");
	}while(strcmp(buffer, "TORNEOOFF") && strcmp(buffer, "TORNEOON"));
	//puts(buffer);
	return strcmp(buffer, "TORNEOOFF");
}

void enviar(const char *palabra){
	char buffer[TAMBUF];
	strcpy(buffer, palabra); //peticiones al servidor
	send(player1.socket, buffer, TAMBUF, 0);
}

void inicializarVariablesInicio(){
	inicializarPlayer(&player1);
	inicializarPlayer(&player2);
	exitPPal=1;
	singlePlayer=0;
	stage=1;
	level=1;
	tortaComida=1; //por si la torta aparece desde el principio 
	inicializarRectangulo(&rectTorta,0,0,0,0);
}

void inicializarPlayer(struct player *playerP){
	playerP->inmunidad=0;
	playerP->cantidadVidas=3;
	playerP->puntos=0;
	playerP->bloqueo=0;
	playerP->ranking=0;
}

void loadAssets(){
	if(TTF_Init()!=0) {
		printf("Error al iniciar TTF: %s\n", TTF_GetError());
		exit (-1);
    }
	if((tipo_letra = TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 15)) == NULL) {
		printf("Error al abrir fuente: %s\n", TTF_GetError());
		exit (-1);
	}
	inicializarRectangulo(&rectStage, 0, 325, 506, 448); 
	cargarImagen(&mapa, "./ImagenesSDL/mapa.jpg");
	cargarImagen(&plantilla, "./ImagenesSDL/spritesFelix.png");
	inicializarRectangulo(&rectFelix, 0, 0, 29, 65);
	inicializarRectangulo(&rectFelixRed, 0, 65, 29, 65);
	inicializarRectangulo(&rectRalph,0,169,87,104);
	inicializarRectangulo(&rectDestinoRalph,posX[6]-33,posY[0]-86,0,0);	
}

void pantallaCarga(){
	SDL_Rect rect;
	SDL_Surface * texto;
	int result=1;
	while(result){ //while que sirve para esperar a la otra persona, con esto estamos 100% seguros
		SDL_FillRect(pantalla, NULL, SDL_MapRGB(pantalla->format, 0, 0, 0));
		inicializarRectangulo(&rect, 200, 250, 0, 0);
		texto = TTF_RenderText_Blended(TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 20), "Loading ...", newColor(255,255,255));
		SDL_BlitSurface(texto,NULL,pantalla,&rect);
		SDL_Flip(pantalla);
		result=0;
		if(rectDestinoFelix.x == rectDestinoFelix.y ) //hasta que no se reciba la posicion de felix no se empieza
			result ++;
		if(rectDestinoFelixRed.x == rectDestinoFelixRed.y) //hasta que no se reciba la posicion de felixRed no se empieza
			result ++;
		SDL_Delay(100);
	}
	SDL_FreeSurface(texto);
}

void esperaRival(int socket){
	char buffer[TAMBUF];
	int esperando=1;
	
	pthread_t threadAnimacion;
	pthread_create(&threadAnimacion, NULL,&animatorWaiting, (void *)&esperando);
	pthread_t threadEvent;
	pthread_create(&threadEvent, NULL,&exitListener, NULL);
	while(esperando){ //while que sirve para esperar a la otra persona, con esto estamos 100% seguros
		bzero(buffer, TAMBUF); //rellena con ceros el resto de la estructura
		if(recv(socket, buffer, TAMBUF, 0)<=0){
			printf("El servidor del torneo se ha caido!\n");
			exit(0);
		}
		if(strcmp(buffer, "conectado")==0) //verifica lo que se recibio
			esperando=0;
		else if(strcmp(buffer, "FIN") == 0){
			esperando = 0;
			abandono = 1;
		}
		else{ //recibe el ranking actualizado
			int p;
			int x;
			int y;
			sscanf(buffer, "%d %d %d", &p, &x, &y);
			if(p==1 && x==RANKING)
				player1.ranking= y;
		}
	}
	pthread_cancel(threadEvent);
	pthread_join(threadAnimacion, NULL);
}

void * exitListener(void * args){
	SDL_Event evento;
	//printf("ENTRO AL LISTENER");
	while (SDL_WaitEvent(&evento))
		if(evento.type== SDL_QUIT){
			//printf("Juego Terminado!\n");
			exit(0);
		}
}

void * animatorWaiting(void * args){
	SDL_Rect rect;
	SDL_Rect rectDestino;
	SDL_Surface * texto;
	int alternancia=1;
	int cont=0;
	char mensaje[]= "Waiting for opponent ";
	char aux[30];
	while(*((int *) args)){
		generarMarco(20, 20, 466, 486, newColor(255,0,0));
		strcpy(aux, mensaje);
		for (int i = 0; i < cont; ++i)
			strcat(aux, ".");
		cont ++;
		if(cont==4)
			cont=0;
		inicializarRectangulo(&rect, 125, 350, 0, 0);
		texto = TTF_RenderText_Blended(TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 20), aux, newColor(255,255,255));
		SDL_BlitSurface(texto,NULL,pantalla,&rect);
		inicializarRectangulo(&rectDestino, 200+alternancia*10, 150, 0, 0);
		inicializarRectangulo(&rect, 172+alternancia*111, 353, 111-alternancia*18, 144);
		SDL_BlitSurface(plantilla,&rect,pantalla,&rectDestino);	
		if(alternancia==1)
			alternancia=0;
		else
			alternancia=1;

		inicializarRectangulo(&rect, 15, 15, 85, 30);
		SDL_FillRect(pantalla, &rect, SDL_MapRGB(pantalla->format, 255, 255, 255));
		inicializarRectangulo(&rect, 15+3, 15+3, 85-6, 30-6);
		SDL_FillRect(pantalla, &rect, SDL_MapRGB(pantalla->format, 0, 0, 0));
		inicializarRectangulo(&rect, 24, 22, 0, 0);
		
		sprintf(aux, "RANK: %d", player1.ranking);
		if(player1.ranking == 0)
			texto = TTF_RenderText_Blended(tipo_letra, "RANK: ", newColor(254, 239, 51));
		else
			texto = TTF_RenderText_Blended(tipo_letra, aux, newColor(254, 239, 51));
		SDL_BlitSurface(texto,NULL,pantalla,&rect);
		SDL_Delay(350);
		SDL_Flip(pantalla);
	}
	SDL_FreeSurface(texto);
}

int handleEvent(SDL_Event evento){
	int delay=0;
	//printf("Entro al manejador!\n");
	if(player1.bloqueo!=1){
		switch (evento.type){
			case SDL_KEYDOWN:
				if(evento.key.keysym.sym== player1.configuracion.up){
					enviar("UP");
					delay= 150;
				}
				else if(evento.key.keysym.sym== player1.configuracion.down){
					enviar("DOWN");
					delay= 150;
				}
				else if(evento.key.keysym.sym== player1.configuracion.left){
					enviar("LEFT");
					delay= 150;
				}
				else if(evento.key.keysym.sym== player1.configuracion.right){
					enviar("RIGHT");
					delay= 150;
				}
				else if(evento.key.keysym.sym== player1.configuracion.action){
					enviar("HAMMER");
					delay= 600;
				}
				else if(evento.key.keysym.sym== SDLK_ESCAPE){
					enviar("EXIT");
					abandono = 2;
					exitPPal=0;
				}
			break;
			case SDL_QUIT:
				enviar("EXIT");
				abandono = 2;
				exitPPal=0;
			break;
		}
		while(SDL_PollEvent(&evento)); //limpio la cola de eventos
	}
	return delay;
}

void dibujarObjetos(){
	SDL_Rect rect;
	SDL_Rect rectDestino;
	for (int i = 0; i < 15; ++i){//imprimo ventanas dentro del juego
		if(ventanas[i]!=0){ //habia algun tipo de ventana
			if(i==7 && stage==1) //si estoy en la ventana 8, y en el 1er stage
				inicializarRectangulo(&rect, 256 + 61 *(ventanas[i]-1), 54, 61, 66);
			else if(i==12 && stage==1) //si estoy en la ventana 12, y en el 1er stage
				inicializarRectangulo(&rect, 256 + 58 *(ventanas[i]-1), 0, 58, 54);
			else
				inicializarRectangulo(&rect, 256 + 50 *(ventanas[i]-1), 120, 50, 86);
			if(i>=0 && i < 5) //primera fila
				inicializarRectangulo(&rectDestino, posX[i*2+2]-14, posY[1]-74, 50, 86);
			else if(i>=5 && i < 10){ //segunda fila
				if(i==7 && stage==1)
					inicializarRectangulo(&rectDestino, posX[(i-5)*2+2]- 19.5, posY[3]-66, 50, 86);	
				else
					inicializarRectangulo(&rectDestino, posX[(i-5)*2+2]-14, posY[3]-74, 50, 86);
			} 
			else {
				if(stage==1){ //era la planta baja
					if(i==12)
						inicializarRectangulo(&rectDestino, posX[(i-10)*2+2]- 18, posY[5]- 50, 50, 86);	
					else
						inicializarRectangulo(&rectDestino, posX[(i-10)*2+2]- 14, posY[5]-74 -18, 50, 86);
				} 
				else
					inicializarRectangulo(&rectDestino, posX[(i-10)*2+2]-14, posY[5]-74, 50, 86);
			}
			SDL_BlitSurface(plantilla, &rect, pantalla, &rectDestino);
		}

		if(marcos[i]!=0 && i<12){
			inicializarRectangulo(&rect, 236 + 10 *(marcos[i]-1), 16, 10, 78);
			if(i>=0 && i < 4) //primera fila
				inicializarRectangulo(&rectDestino, posX[i*2+3] +5, posY[1]-74, 50, 86);
			else if(i>=4 && i < 8) //segunda fila
				inicializarRectangulo(&rectDestino, posX[(i-4)*2+3] +5, posY[3]-74, 50, 86);
			else 
				inicializarRectangulo(&rectDestino, posX[(i-8)*2+3] +5, posY[5]-74, 50, 86);
			SDL_BlitSurface(plantilla, &rect, pantalla, &rectDestino);	
		}

		if(techos[i]!=0 && i<10){
			inicializarRectangulo(&rect, 173, 16, 50, 12);
			if(i>=0 && i < 5) //primera fila
				inicializarRectangulo(&rectDestino, posX[i*2+2]-14, posY[2] -15, 50, 86);
			else 
				inicializarRectangulo(&rectDestino, posX[(i-5)*2+2]-14, posY[4] -15, 50, 86);
			SDL_BlitSurface(plantilla, &rect, pantalla, &rectDestino);
		}
		SDL_BlitSurface(plantilla, &rectTorta, pantalla, &rectDestinoTorta);
	}
	inicializarRectangulo(&rect, 213, 36, 20, 13); //carga de vidas P1
	for (int i = 0; i < player1.cantidadVidas; ++i){
		inicializarRectangulo(&rectDestino, 138+i*21, 504, 20, 13);
		SDL_BlitSurface(plantilla, &rect, pantalla, &rectDestino);
	}
	inicializarRectangulo(&rect, 213, 49, 20, 13);
	for (int i = 0; i < player2.cantidadVidas; ++i){
		inicializarRectangulo(&rectDestino, 354+i*21, 504, 20, 13);
		SDL_BlitSurface(plantilla, &rect, pantalla, &rectDestino);
	}
}

void calcularPosiciones(){
	if(stage==1){
		if(rectDestinoFelix.y == posY[5]-66){ //si estoy en la ultima fila
			if(rectDestinoFelix.x== posX[6]-10) //si estoy parado en el medio, entonces tengo que estar en la puerta
				rectDestinoFelix.y= posY[5]-66+5;
			else
				rectDestinoFelix.y= posY[5]-66-17;
		}
		if(rectDestinoFelixRed.y == posY[5]-66){
			if(rectDestinoFelixRed.x== posX[6]+5)
				rectDestinoFelixRed.y= posY[5]-66+5;
			else
				rectDestinoFelixRed.y= posY[5]-66-17;
		}
	}
}

void cambioStage(){
	char buffer[30];
	SDL_Rect rect;
	SDL_Surface * texto;
	SDL_FillRect(pantalla, NULL, SDL_MapRGB(pantalla->format, 0, 0, 0));
	inicializarRectangulo(&rect, 200, 250, 0, 0);
	sprintf(buffer, "Level: %d ", level);
	texto = TTF_RenderText_Blended(TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 20), buffer, newColor(255,255,255));
	SDL_BlitSurface(texto,NULL,pantalla,&rect);
	inicializarRectangulo(&rect, 225, 300, 0, 0);
	sprintf(buffer, "Stage: %d ", stage);
	texto = TTF_RenderText_Blended(TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 20), buffer, newColor(255,255,255));
	SDL_BlitSurface(texto,NULL,pantalla,&rect);
	SDL_Flip(pantalla);
	SDL_FreeSurface(texto);
}

void * render(void * args){
	unsigned int lastTime[]={0,0};
	unsigned int currentTime[2];
	int stageAux= stage;
	while(exitPPal){
		if(stageAux!=stage){
			stageAux= stage;
			cambioStage();
			SDL_Delay(1500);
		}
		currentTime[0]= SDL_GetTicks(); //obtiene la cantidad de milisegundos desde que se inicializo SDL
		currentTime[1]= SDL_GetTicks();
		SDL_BlitSurface(mapa, &rectStage, pantalla, NULL);
		SDL_BlitSurface(plantilla, &rectRalph, pantalla, &rectDestinoRalph);
		actualizarHud();
		dibujarObjetos();
		calcularPosiciones();
		if(player1.inmunidad!=0){ //si la inmunidad estaba activa
			if(currentTime[0]%2==0){
				SDL_BlitSurface(plantilla, &rectFelix, pantalla, &rectDestinoFelix);
				if(player1.berserker==1)
					dibujarGorro(&rectFelix, &rectDestinoFelix);
			}
			if(currentTime[0]> lastTime[0]+1000){ //paso un segundo
				lastTime[0]= currentTime[0];
				player1.inmunidad --;
			} 
		} else {
			SDL_BlitSurface(plantilla, &rectFelix, pantalla, &rectDestinoFelix);
			if(player1.berserker==1)
				dibujarGorro(&rectFelix, &rectDestinoFelix);
		}
		if(singlePlayer==0){ //si estaba jugando multiplayer
			if(player2.inmunidad!=0){
				if(currentTime[1]%2==0){
					SDL_BlitSurface(plantilla, &rectFelixRed, pantalla, &rectDestinoFelixRed);	
					if(player2.berserker==1)
						dibujarGorro(&rectFelixRed, &rectDestinoFelixRed);
				}
				if(currentTime[1]> lastTime[1]+1000){ //paso un segundo
					lastTime[1]= currentTime[1];
					player2.inmunidad --;
				}
			}else{
				SDL_BlitSurface(plantilla, &rectFelixRed, pantalla, &rectDestinoFelixRed);
				if(player2.berserker==1)
					dibujarGorro(&rectFelixRed, &rectDestinoFelixRed);
			}
		}
		for (int i = 0; i < 3; ++i){
			if(i<2)
				SDL_BlitSurface(plantilla, &rectPuntuacion[i], pantalla, &rectDestinoPuntuacion[i]);
			SDL_BlitSurface(plantilla, &rectPajaro[i], pantalla, &rectDestinoPajaro[i]);
			SDL_BlitSurface(plantilla, &rectLadrillo[i], pantalla, &rectDestinoLadrillo[i]);		
		}
		SDL_Flip(pantalla);
		SDL_Delay(100);
	}
	//printf("Se murio el render\n");
}

void dibujarGorro(SDL_Rect * rectF, SDL_Rect * rectDestF){
	SDL_Rect rectDestinoGorro;
	switch(rectF->x){
		case 0:
			inicializarRectangulo(&rectDestinoGorro, rectDestF->x +4, rectDestF->y,0,0);
		break;
		case 29:
			inicializarRectangulo(&rectDestinoGorro, rectDestF->x +8, rectDestF->y,0,0);
		break;
		case 69:
			inicializarRectangulo(&rectDestinoGorro, rectDestF->x +8, rectDestF->y +1,0,0);
		break;
		case 117:
			inicializarRectangulo(&rectDestinoGorro, rectDestF->x +4, rectDestF->y +1,0,0);
		break;
	}
	SDL_BlitSurface(plantilla, &rectGorro, pantalla, &rectDestinoGorro);
}

void actualizarHud(){
	SDL_Rect rect;
	SDL_Rect aux;
	SDL_Surface * texto;
	char contenido[10];

	inicializarRectangulo(&rect, 0, 660, 506, 78);
	inicializarRectangulo(&aux, 0,448,0,0);
	SDL_BlitSurface(plantilla, &rect, pantalla, &aux); //pone la imagen del hud

	inicializarRectangulo(&rect, 90, 468, 0, 0);
	sprintf(contenido, "%d", player1.puntos);
	texto = TTF_RenderText_Blended(tipo_letra, contenido, newColor(254, 239, 51));
	SDL_BlitSurface(texto,NULL,pantalla,&rect);

	inicializarRectangulo(&rect, 138, 485, 0, 0);
	texto = TTF_RenderText_Blended(tipo_letra, player1.configuracion.nick, newColor(254, 239, 51));
	SDL_BlitSurface(texto,NULL,pantalla,&rect);

	inicializarRectangulo(&rect, 306, 468, 0, 0);
	sprintf(contenido, "%d", player2.puntos);
	texto = TTF_RenderText_Blended(tipo_letra, contenido, newColor(254, 239, 51));
	SDL_BlitSurface(texto,NULL,pantalla,&rect);

	inicializarRectangulo(&rect, 354, 485, 0, 0);
	texto = TTF_RenderText_Blended(tipo_letra, player2.configuracion.nick, newColor(254, 239, 51));
	SDL_BlitSurface(texto,NULL,pantalla,&rect);

	inicializarRectangulo(&rect, 15, 15, 85, 30);
	SDL_FillRect(pantalla, &rect, SDL_MapRGB(pantalla->format, 255, 255, 255));
	inicializarRectangulo(&rect, 15+3, 15+3, 85-6, 30-6);
	SDL_FillRect(pantalla, &rect, SDL_MapRGB(pantalla->format, 0, 0, 0));
	
	inicializarRectangulo(&rect, 24, 22, 0, 0);

	sprintf(contenido, "RANK: %d", player1.ranking);
	if(player1.ranking == 0)
		texto = TTF_RenderText_Blended(tipo_letra, "RANK: ", newColor(254, 239, 51));
	else
		texto = TTF_RenderText_Blended(tipo_letra, contenido, newColor(254, 239, 51));
	SDL_BlitSurface(texto,NULL,pantalla,&rect);

	SDL_FreeSurface(texto);
}

void newPackage(struct packageAnimator * pack, SDL_Rect * rect, SDL_Rect * rectDestino, const char *orden){
	pack->rect= rect;
	pack->rectDestino= rectDestino;
	strcpy(pack->orden, orden);
}

void * animator(void * args){
	struct packageAnimator * aux = (packageAnimator *) args; 
	struct packageAnimator pack;
	newPackage(&pack, aux->rect, aux->rectDestino, aux->orden);
	if(strcmp(pack.orden, "desplazar")==0){
		if(compareRect(pack.rectDestino, &rectDestinoFelixRed)==1){ //era el otro felix
			if(player2.bloqueo==0)
				inicializarRectangulo(pack.rect, 29, 65, 40, 65);
			SDL_Delay(250);
			if(player2.bloqueo==0)
				inicializarRectangulo(pack.rect, 0, 65, 29, 65);	
		}
		else{
			if(player1.bloqueo==0) //si no esta bloqueado
				inicializarRectangulo(pack.rect, 29, 0, 40, 65);
			SDL_Delay(250);
			if(player1.bloqueo==0)
				inicializarRectangulo(pack.rect, 0, 0, 29, 65);
		}
	}
	else if(strcmp(pack.orden, "hammer")==0){
		if(compareRect(pack.rectDestino, &rectDestinoFelixRed)==1){ //era el otro felix
			for (int i = 0; i < 2; ++i) {
				if(player2.bloqueo==0)
					inicializarRectangulo(pack.rect, 69, 65, 48, 65);
				SDL_Delay(150);
				if(player2.bloqueo==0)
					inicializarRectangulo(pack.rect, 117, 65, 56, 65);
				SDL_Delay(150);
			}
			if(player2.bloqueo==0)
				inicializarRectangulo(pack.rect, 0, 65, 29, 65);	
		} 
		else{
			for (int i = 0; i < 2; ++i) {
				if(player1.bloqueo==0)
					inicializarRectangulo(pack.rect, 69, 0, 48, 65);
				SDL_Delay(150);
				if(player1.bloqueo==0)
					inicializarRectangulo(pack.rect, 117, 0, 56, 65);
				SDL_Delay(150);
			}
			if(player1.bloqueo==0)
				inicializarRectangulo(pack.rect, 0, 0, 29, 65);
		}

	}
	else if(strcmp(pack.orden, "moverPajaroIzqDer")==0){
		int mov= (int) (tiempoPatos*1000)/(435-50);
		int stageAux= stage;
		for (int i = 20; i < 435 && stageAux== stage; i++){
			if(i%5==0)
				if(i%2==0)
					inicializarRectangulo(pack.rect, 0, 131, 42, 38);
				else
					inicializarRectangulo(pack.rect, 42, 131, 42, 38);
			pack.rectDestino->x= i;
			SDL_Delay(mov);
		}
		inicializarRectangulo(pack.rect,0, 0, 0, 0);
	}
	else if(strcmp(pack.orden, "moverPajaroDerIzq")==0){
		int mov= (int) (tiempoPatos*1000)/(435-50);
		int stageAux= stage;
		for (int i = 445; i > 51 && stageAux== stage && exitPPal==1; i--){
			if(i%5==0)
				if(i%2==0)
					inicializarRectangulo(pack.rect, 84, 131, 42, 38);
				else
					inicializarRectangulo(pack.rect, 126, 131, 42, 38);
			pack.rectDestino->x= i;
			SDL_Delay(mov);
		}
		inicializarRectangulo(pack.rect,0, 0, 0, 0);
	}
	else if(strcmp(pack.orden, "caeLadrillo")==0){ //that logic e.e
		float mov=(448-122)*50 / tiempoLadrillos;
		inicializarRectangulo(pack.rect, 173, 0, 20, 16);
		int stageAux= stage;
		for (int i = 122; i < 448 && stageAux== stage && exitPPal==1; i+=mov){
			if(i%5==0)
				if(i%2==0)
					inicializarRectangulo(pack.rect, 173, 0, 20, 16);
				else
					inicializarRectangulo(pack.rect, 193, 0, 20, 16);
			pack.rectDestino->y= i;
			SDL_Delay(50);
			if(numLadrillo!=0) //si algun player choco con algun ladrillo
				if(compareRect(pack.rectDestino, &rectDestinoLadrillo[numLadrillo-1])==1){ //si es el actual
					numLadrillo=0; //restauro la variable
					break; //corto el ciclo
				}
		}
		inicializarRectangulo(pack.rect,0, 0, 0, 0);
	}
	else if(strcmp(pack.orden, "puntuacionUp")==0){
		int posAux[2];
		int player=1;
		if(compareRect(pack.rectDestino, &rectDestinoFelixRed)==1)
			player= 2; //puntuacion p1
		inicializarRectangulo(&rectPuntuacion[player-1], 213, 0, 43, 16);
		posAux[0]= pack.rectDestino->x; //guardo las posiciones auxiliares
		posAux[1]= pack.rectDestino->y;
		pack.rect->x= posAux[0]+20;
		for (int i = 0; i < 12; i++){
			pack.rect->y= posAux[1] -i;
			SDL_Delay(50);
		}
		inicializarRectangulo(&rectPuntuacion[player-1], 0, 0, 0, 0);
	}
	else if(strcmp(pack.orden, "moverRalph")==0){
		int dif = pack.rect->x - pack.rectDestino->x;
		tiempoRalph= tiempoRalph/1000000;
		float delay= (float)1000/(32*tiempoRalph);
		float sumaDelay=0;
		pack.rect->y = posY[0]-86 -9;
		int stageAux= stage;
		if(dif > 0 ) //derecha a izquierda
			for(int i = pack.rect->x ; i > pack.rectDestino->x-33 && sumaDelay<tiempoRalph*1000 && exitPPal==1 && stageAux== stage; i--){
				if(i%5==0)
					if(i%2==0)
						inicializarRectangulo(&rectRalph,0,273,86,112);
					else
						inicializarRectangulo(&rectRalph,86,273,86,112);
				pack.rect->x = i;
				sumaDelay+= delay;
				SDL_Delay(delay);
			}
		else{
			for(int i = pack.rect->x ; i < pack.rectDestino->x-33 && sumaDelay<tiempoRalph*1000 && stageAux== stage && exitPPal==1; i++){
				if(i%5==0)
					if(i%2==0)
						inicializarRectangulo(&rectRalph,0,385,86,112);
					else
						inicializarRectangulo(&rectRalph,86,385,86,112);
				pack.rect->x  = i;
				sumaDelay+= delay;
				SDL_Delay(delay);
			}
		}
		pack.rect->y = posY[0]-86-13;
		pack.rect->x = pack.rectDestino->x-55;
		for (int i = 0; i < 12 && stageAux== stage; ++i){
			if(i%2==0)
				inicializarRectangulo(&rectRalph,372,206,132,116);
			else
				inicializarRectangulo(&rectRalph,240,206,132,116);
			SDL_Delay(400);
		}
		pack.rect->x = pack.rectDestino->x-33;
		pack.rect->y = posY[0]-86;
		stageAux= stage;
		inicializarRectangulo(&rectRalph,0,169,87,104);	
	}
	else if(strcmp(pack.orden, "berserker")==0){
		int cant= tiempoBerserker*1000/100;
		// printf("cant= %d\n", cant);
		if(compareRect(pack.rectDestino, &rectDestinoFelixRed)) //el otro felix se comio la torta
			player2.berserker= 1;
		else
			player1.berserker=1;
		for (int i = 0; i < cant && exitPPal==1; ++i){
			inicializarRectangulo(&rectGorro, 173 , 82+ (i%4)*13, 24, 13);
			SDL_Delay(100);
		}
		player1.berserker=0;
		player2.berserker=0;
		inicializarRectangulo(&rectGorro, 0, 0, 0, 0);
	}
	else if(strcmp(pack.orden, "perderVida")==0){
		if(compareRect(pack.rectDestino, &rectDestinoFelixRed)){ //el otro felix se comio la torta
			inicializarRectangulo(pack.rect, 117, 206, 30, 67);
			player2.bloqueo=1;
			SDL_Delay(2000);
			player2.bloqueo=0;
			if(player2.cantidadVidas>0){
				player2.inmunidad= tiempoInmunidad-2;
				inicializarRectangulo(pack.rect, 0, 65, 29, 65);
			}
			else{
				pack.rectDestino->x -= 10;
				pack.rectDestino->y += 35;
				inicializarRectangulo(pack.rect, 147, 237, 211-147, 273-237);
			}
		} 
		else{
			inicializarRectangulo(pack.rect, 87, 206, 30, 67);
			player1.bloqueo=1;
			SDL_Delay(2000);
			player1.bloqueo=0;
			if(player1.cantidadVidas>0){
				player1.inmunidad= tiempoInmunidad-2;
				inicializarRectangulo(pack.rect, 0, 0, 29, 65);	
			}
			else{ //se le acabaron las vidas!
				pack.rectDestino->x -= 10;
				pack.rectDestino->y += 35;
				inicializarRectangulo(pack.rect, 147, 237, 211-147, 273-237);		
			}
		}
	}
	else if(strcmp(pack.orden, "mostrarTorta")==0){
		inicializarRectangulo(pack.rect, 173, 134, 24, 20);
		pack.rectDestino->y -= 24;
		tortaComida=0;
		SDL_Delay(3000);
		while(!tortaComida){
			inicializarRectangulo(pack.rect, 213, 62, 22, 26);
			SDL_Delay(500);
			inicializarRectangulo(pack.rect, 213, 88, 22, 26);
			SDL_Delay(500);
		}
		inicializarRectangulo(pack.rect, 0, 0, 0, 0);
	}
}

void * listener(void * args){
	char buffer[TAMBUF];
	pthread_t threadA;
	pthread_attr_t attr; // thread attribute
 	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	while(1){
		bzero(buffer, TAMBUF); //rellena con ceros el resto de la estructura
		recv(player1.socket, buffer, TAMBUF, 0); //receive bloqueante
		//printf("%s ::: %d\n",buffer, (int)strlen(buffer));
		if(strcmp(buffer, "FIN")==0){ //recibi la aceptacion de salida
			abandono = 1;
			break;
		}
		else if(strcmp(buffer, "PARTIDAOFF")==0){//recibi la aceptacion de salida
			printf("La partida se ha caido!\n");
			exitPPal=0;
			exit(-1);
			break;
		} 
		else if(strcmp(buffer, "1 EXIT")==0) //recibi la aceptacion de salida
			break;
		else if(strcmp(buffer, "2 EXIT")==0){ //el otro jugador abandono la sala 
			singlePlayer=1;
			player2.cantidadVidas= 0;
		} 
		else if(strlen(buffer)!=19)
			printf("Se recibio basura\n");
		else {//verifica lo que se recibio 
			int p;
			int x;
			int y;
			sscanf(buffer, "%d", &p);
			switch(p){
				case CAMBIO_NIVEL:
					// printf("PASO NIVEL\n");
					level ++;
					stage=1;
					inicializarRectangulo(&rectStage, 0, 325, 506, 448);
					tortaComida=1;
					inicializarRectangulo(&rectDestinoRalph,posX[6]-33,posY[0]-86,0,0);	
				break;
				case CAMBIO_STAGE: //se cambia a la siguiente stage
					// printf("PASE PISO\n");
					if(stage==1){ //por si estaba en el primer nivel, tiene que recalcular las posiciones
						if(rectDestinoFelix.y== 347 || rectDestinoFelix.y== 369)
							rectDestinoFelix.y = posY[5]-66;
						if(rectDestinoFelixRed.y== 347 || rectDestinoFelixRed.y== 369)
							rectDestinoFelixRed.y = posY[5]-66;
					}
					stage++;
					tortaComida= 1;
					inicializarRectangulo(&rectDestinoRalph,posX[6]-33,posY[0]-86,0,0);	
					inicializarRectangulo(&rectStage, 0,0,506, 448);
				break;
				case PLAYER1: //se modifica el player1 - Felix
					struct packageAnimator packP1;
					sscanf(buffer, "%d %d", &p, &x);
					if(x<11){ //se mando posicion dentro de la matriz
						sscanf(buffer, "%d %d %d", &p, &x, &y);
						if(player1.cantidadVidas!=0){
						rectDestinoFelix.x= posX[x+1]-10;
						rectDestinoFelix.y= posY[y]-66;
							newPackage(&packP1, &rectFelix, &rectDestinoFelix, "desplazar");
							pthread_create(&threadA,&attr,&animator, &packP1);
						}
					}
					else if(x==VIDA){ //se recibio modificacion de vida
						struct packageAnimator packP1Aux;
						sscanf(buffer, "%d %d %d %d %d", &p, &x, &player1.cantidadVidas, &tiempoInmunidad, &numLadrillo);
						if(tiempoInmunidad!=0)
							if(player1.inmunidad==0){ //si no tenian la inmunidad
								newPackage(&packP1Aux, &rectFelix, &rectDestinoFelix, "perderVida");
								pthread_create(&threadA,&attr,&animator, &packP1Aux);
							}
					}
					else if(x==HAMMER){ //si es H, (se apreto el hammer)
						newPackage(&packP1, &rectFelix, &rectDestinoFelix, "hammer");
						pthread_create(&threadA,&attr,&animator, &packP1);
					} 
					else if(x==TORTA){
						struct packageAnimator packP1Torta;
						sscanf(buffer, "%d %d %d", &p, &x, &tiempoBerserker);
						// printf("JUGADOR UNO COMIO TORTA\n");
						inicializarRectangulo(&rectTorta, 0, 0, 0, 0); //se comio la torta
						newPackage(&packP1Torta, &rectGorro, &rectDestinoFelix, "berserker");
						pthread_create(&threadA,&attr,&animator, &packP1Torta);
						tortaComida=1;
					}
					else if(x==RANKING){
						sscanf(buffer, "%d %d %d", &p, &x, &player1.ranking);
					}
				break;
				case PLAYER2: //se modifica el player2 - FelixRed
					struct packageAnimator packP2;
					sscanf(buffer, "%d %d", &p, &x);
					if(x<11){
						sscanf(buffer, "%d %d %d", &p, &x, &y);
						if(player2.cantidadVidas!=0){
						rectDestinoFelixRed.x= posX[x+1]-10+15;
						rectDestinoFelixRed.y= posY[y]-66;
							newPackage(&packP2, &rectFelixRed, &rectDestinoFelixRed, "desplazar");
							pthread_create(&threadA,&attr,&animator, &packP2);
						}
					}
					else if(x==VIDA){ //se recibio modificacion de vida
						sscanf(buffer, "%d %d %d %d %d", &p, &x, &player2.cantidadVidas, &tiempoInmunidad, &numLadrillo);
						if(tiempoInmunidad!=0)
							if(player2.inmunidad==0){ //si no tenian la inmunidad
								newPackage(&packP2, &rectFelixRed, &rectDestinoFelixRed, "perderVida");
								pthread_create(&threadA,&attr,&animator, &packP2);
							}
					} 
					else if(x== HAMMER){
						newPackage(&packP2, &rectFelixRed, &rectDestinoFelixRed, "hammer");
						pthread_create(&threadA,&attr,&animator, &packP2);
					}
					else if(x== TORTA){
						struct packageAnimator packP2Torta;
						sscanf(buffer, "%d %d %d", &p, &x, &tiempoBerserker);
						inicializarRectangulo(&rectTorta, 0, 0, 0, 0); //se comio la torta
						newPackage(&packP2Torta, &rectGorro, &rectDestinoFelixRed, "berserker");
						pthread_create(&threadA,&attr,&animator, &packP2Torta);
						tortaComida=1;
					}
				break;
				case PAJARO:
					struct packageAnimator packPajaro;
					char lado;
					int numPato;
					sscanf(buffer, "%d %d %d %c %d", &p, &y, &tiempoPatos, &lado, &numPato);
					rectDestinoPajaro[numPato-1].y= posY[y]-25-30;
					if(lado=='L')
						newPackage(&packPajaro, &rectPajaro[numPato-1], &rectDestinoPajaro[numPato-1], "moverPajaroIzqDer");
					else if(lado == 'R')
						newPackage(&packPajaro, &rectPajaro[numPato-1], &rectDestinoPajaro[numPato-1], "moverPajaroDerIzq");
					pthread_create(&threadA,&attr,&animator, &packPajaro);
				break;
				case LADRILLO: 
					struct packageAnimator packLadrillo;
					int numLad;
					sscanf(buffer, "%d %d %d %d %d", &p, &x, &y, &numLad, &tiempoLadrillos);
					rectDestinoLadrillo[numLad-1].x= posX[x+1];
					newPackage(&packLadrillo, &rectLadrillo[numLad-1], &rectDestinoLadrillo[numLad-1], "caeLadrillo");
					pthread_create(&threadA,&attr,&animator, &packLadrillo);
				break;
				case RALPH: //se modifica Ralph
					struct packageAnimator packRalph;
					sscanf(buffer, "%d %d %f", &p, &x, &tiempoRalph);
					SDL_Rect rectDestinoFinalRalph;
					inicializarRectangulo(&rectDestinoFinalRalph, posX[x+1], rectDestinoRalph.y, 0 ,0);
					newPackage(&packRalph, &rectDestinoRalph, &rectDestinoFinalRalph, "moverRalph");
					pthread_create(&threadA,&attr,&animator, &packRalph);
				break;
				case VENTANA:
					struct packageAnimator packPuntuacion;
					int estado;
					int player;
					int puntos;
					sscanf(buffer, "%d %d %d %d %d %d", &p, &x, &y, &estado, &player, &puntos);
					ventanas[(x/2)+((y/2)*5)]= estado; //its magic :D
					if(player==1){ //mando la posicion del rectPuntuacion y la posicion del rectFelix
						newPackage(&packPuntuacion, &rectDestinoPuntuacion[player-1], &rectDestinoFelix, "puntuacionUp");
						player1.puntos = puntos;
					}
					else if(player==2){
						newPackage(&packPuntuacion, &rectDestinoPuntuacion[player-1], &rectDestinoFelixRed, "puntuacionUp");
						player2.puntos = puntos;
					}
					pthread_create(&threadA,&attr,&animator, &packPuntuacion);
				break;
				case TORTA:
					struct packageAnimator packTorta;
					sscanf(buffer, "%d %d %d", &p, &x, &y);
					inicializarRectangulo(&rectDestinoTorta, posX[y+1], posY[x], 0, 0);
					newPackage(&packTorta, &rectTorta, &rectDestinoTorta, "mostrarTorta");
					pthread_create(&threadA,&attr,&animator, &packTorta);
				break;
				case CARGAVENTANA:
					for (int i = 0; i < 15; i++)
						ventanas[i]= (int)buffer[i+3]-48;
				break;
				case CARGAMARCO:
					for (int i = 0; i < 12; i++)
						if((int)buffer[i+3]-48 ==1) //si habia un 1 en la posicion del buffer
							marcos[i]= rand()%2 +1; //coloco con random 1 o 2 
						else
							marcos[i]=0;	
				break;
				case CARGATECHO:
					for (int i = 0; i < 10; i++)
						techos[i]= (int)buffer[i+3]-48;
				break;
				case NOMBREPLAYER:
					sscanf(buffer, "%d %s ", &p, player2.configuracion.nick);
				break;
			}
		}
	}
	exitPPal = 0;
	//printf("Se murio el listener\n");
}