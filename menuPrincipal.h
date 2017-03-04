#include <sys/types.h> 
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

int mostrarMenuPrincipal(SDL_Surface * pantalla);
void renderMainScreen(int opcion);
void loadAssetsMenu();
int cargarImagen(SDL_Surface ** imagen, const char * ruta);
void inicializarRectangulo(SDL_Rect * rect, int x,int y,int w,int h);
int handleEventMenu(SDL_Event evento, int * opcion, int render);
int errorMessage(const char *msg);
int getInput(SDL_Event evento, char * cadena);
void renderOptionScreen(int opcion);
int action(int opcion, int render);
int cargarConfig(struct config *configuracion);
int guardarConfig(struct config *configuracion);
int compareRect(SDL_Rect * rect1, SDL_Rect * rect2);
SDL_Color newColor(int r, int g, int b);
void mostrarTecla(int valor, char * aux);
int errorMessage(const char *msg);
int getNumbers(SDL_Event evento, char * cadena);
int verificarTeclas(int tecla);
void generarArchivoDefault();
void generarMarco(int x, int y, int tamX, int tamY, SDL_Color color);
Uint32 colorToMap(SDL_Color color);
void escribirEnPantalla(char * content, int x, int y, TTF_Font *font, SDL_Color color, SDL_Surface *screen);

//PROTOTIPOS
void leerVentanas(char *buffer);
void leerMarcos(char *buffer);
void leerTechos(char *buffer);
void * animator(void * args);
void * listener(void * args);
int handleEvent(SDL_Event evento);
void createAnimation(int opt);
void dibujarObjetos();
void * render(void * args);
void esperaRival(int socket);
void loadAssets();
void actualizarHud();
void calcularPosiciones();
void newPackage(struct packageAnimator * pack, SDL_Rect * rect, SDL_Rect * rectDestino, const char *orden);
void pantallaCarga();
void dibujarGorro(SDL_Rect * rectF, SDL_Rect * rectDestF);
void inicializarVariablesInicio();
void enviar(const char *palabra);
int esperaRespuestaTorneo();
void inicializarPlayer(struct player *playerP);


struct config{
	char IP[30];
	uint port;
	int up;
	int down;
	int right;
	int left;
	int action;
	char nick[10];
};