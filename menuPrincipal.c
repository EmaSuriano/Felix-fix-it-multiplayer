#include "menuPrincipal.h"

#define WinX 506
#define WinY 526

SDL_Surface * screen;
SDL_Surface * plantilla1;
SDL_Rect rectTitle;
TTF_Font *font;

struct config configuracion;

int mostrarMenuPrincipal(SDL_Surface * pantalla){
	int opcion=1;
	screen= pantalla;
	SDL_Event evento;
	loadAssetsMenu();
	renderMainScreen(opcion);
	while (SDL_WaitEvent(&evento))
		switch(handleEventMenu(evento, &opcion,0)){
			case 1:
				return 0;
			case 2:
				return 1;
		}
	return 1;
}

void escribirEnPantalla(char * content, int x, int y, TTF_Font *font, SDL_Color color){
	SDL_Rect rectAux;
	SDL_Surface * texto;
	inicializarRectangulo(&rectAux, x, y, 0, 0);
	texto = TTF_RenderText_Blended(font, content, color);
	SDL_BlitSurface(texto,NULL,screen,&rectAux);
	SDL_FreeSurface(texto);
}

int errorMessage(const char *msg){
	printf("%s\n",msg);
	exit(-1);
}

int getKey(SDL_Event evento){
	if(evento.type == SDL_KEYDOWN)
		return evento.key.keysym.sym;
	return 0;
}

int action(int opcion, int render){
	if(render==0){ //mainMenuScreen
		if(opcion==1)
			return 1;
		else{ //opcion== 2
			int opcion2= 1;
			SDL_Event evento;
			cargarConfig(&configuracion);
			renderOptionScreen(opcion2);
			while (SDL_WaitEvent(&evento))
				if(handleEventMenu(evento, &opcion2,1)!=0)
					break;
			renderMainScreen(opcion);
		}
	}
	else{ //OptionScreen
		SDL_Event evento;
		int aux=0;
		char cadena[100]= "";
		SDL_EnableUNICODE(1);
		switch(opcion){
			case 1:
				strcpy(cadena, configuracion.IP);
				while (SDL_WaitEvent(&evento)){
					if(getNumbers(evento, cadena)==0)
						strcpy(configuracion.IP, cadena);
					else 
						break;
					renderOptionScreen(1);
				}
			break;
			case 2:
				sprintf(cadena, "%d", configuracion.port);
				while (SDL_WaitEvent(&evento)){
					if(getNumbers(evento, cadena)==0)
						configuracion.port= atoi(cadena);
					else 
						break;
					renderOptionScreen(2);
				}
			break;
			case 3:
				while (SDL_WaitEvent(&evento))
					if((aux=getKey(evento))!=0)
						break;
				if(verificarTeclas(aux)==0)
					configuracion.up= aux;
				renderOptionScreen(3);
			break;
			case 4:
				while (SDL_WaitEvent(&evento))
					if((aux=getKey(evento))!=0)
						break;
				if(verificarTeclas(aux)==0)
					configuracion.down= aux;
				renderOptionScreen(4);
			break;
			case 5:
				while (SDL_WaitEvent(&evento))
					if((aux=getKey(evento))!=0)
						break;
				if(verificarTeclas(aux)==0)
					configuracion.right= aux;
				renderOptionScreen(5);
			break;
			case 6:
				while (SDL_WaitEvent(&evento))
					if((aux=getKey(evento))!=0)
						break;
				if(verificarTeclas(aux)==0)
					configuracion.left= aux;
				renderOptionScreen(6);
			break;
			case 7:
				while (SDL_WaitEvent(&evento))
					if((aux=getKey(evento))!=0)
						break;
				if(verificarTeclas(aux)==0)
					configuracion.action= aux;
				renderOptionScreen(7);
			break;
			case 8:
				strcpy(cadena, configuracion.nick);
				while (SDL_WaitEvent(&evento)){
					if(getInput(evento, cadena)==0)
						strcpy(configuracion.nick, cadena);
					else 
						break;
				renderOptionScreen(8);
				}
			break;
			case 9:
				guardarConfig(&configuracion);
				return 1;
			break;
		}
		SDL_EnableUNICODE(0);
	}
	return 0;
}

int verificarTeclas(int tecla){
	if(configuracion.up== tecla || 
		configuracion.down== tecla ||
		configuracion.left== tecla ||
		configuracion.right== tecla ||
		configuracion.action== tecla )
		return 1;
	return 0;
}

int guardarConfig(struct config *configuracion){
	FILE *fp;
	fp=fopen("config.cfg", "wt");
	fprintf(fp, "Ip: %s\n", configuracion->IP);
	fprintf(fp, "Port: %d\n", configuracion->port);
	fprintf(fp, "Up: %d\n", configuracion->up);
	fprintf(fp, "Down: %d\n", configuracion->down);
	fprintf(fp, "Right: %d\n", configuracion->right);
	fprintf(fp, "Left: %d\n", configuracion->left);
	fprintf(fp, "Action: %d\n", configuracion->action);
	fprintf(fp, "Nick: %s", configuracion->nick);
	fclose(fp);
	return 0;
}

int cargarConfig(struct config *configuracion){
	FILE *fp;
	char aux[30];
	fp=fopen("config.cfg", "rt");
	if (fp==NULL){ //si el archivo no existia
		generarArchivoDefault();
		fp=fopen("config.cfg", "rt");
	}
	fscanf(fp, "%s %s", aux, configuracion->IP);
	fscanf(fp, "%s %d", aux, &configuracion->port);
	fscanf(fp, "%s %d", aux, &configuracion->up);
	fscanf(fp, "%s %d", aux, &configuracion->down);
	fscanf(fp, "%s %d", aux, &configuracion->right);
	fscanf(fp, "%s %d", aux, &configuracion->left);
	fscanf(fp, "%s %d", aux, &configuracion->action);
	fscanf(fp, "%s %s", aux, configuracion->nick);
	fclose(fp);
	return 0;
}

void generarArchivoDefault(){
	FILE *fp;
	fp=fopen("config.cfg", "wt");
	fprintf(fp, "Ip: 127.0.0.1\n");
	fprintf(fp, "Port: 8080\n");
	fprintf(fp, "Up: 119\n");
	fprintf(fp, "Down: 115\n");
	fprintf(fp, "Right: 100\n");
	fprintf(fp, "Left: 97\n");
	fprintf(fp, "Action: 32\n");
	fprintf(fp, "Nick: Player");
	fclose(fp);
}

int getInput(SDL_Event evento, char * cadena){
	if(evento.type == SDL_KEYDOWN){
	    	if(evento.key.keysym.unicode == (Uint16)' ')
	    		*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;
		else if(evento.key.keysym.unicode == (Uint16)'.')
			*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;
		else if((evento.key.keysym.unicode >= (Uint16)'0') && ( evento.key.keysym.unicode <= (Uint16)'9'))
			*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;
		else if((evento.key.keysym.unicode >= (Uint16)'a') && ( evento.key.keysym.unicode <= (Uint16)'z'))
			*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;
		else if((evento.key.keysym.unicode >= (Uint16)'A') && ( evento.key.keysym.unicode <= (Uint16)'Z'))
			*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;
		else if(evento.key.keysym.sym== SDLK_BACKSPACE)
			*(cadena+strlen(cadena)-1)= (char) '\0';
	    	else if(evento.key.keysym.sym== SDLK_RETURN)
	    		return 1;
    		*(cadena+strlen(cadena))= '\0';
		if(strlen(cadena)>7)
			return 1;
	}
	return 0;
}

int getNumbers(SDL_Event evento, char * cadena){
	if(evento.type == SDL_KEYDOWN){
	    	if(evento.key.keysym.unicode == (Uint16)' ')
			*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;
		else if(evento.key.keysym.unicode == (Uint16)'.')
			*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;						
		else if((evento.key.keysym.unicode >= (Uint16)'0') && ( evento.key.keysym.unicode <= (Uint16)'9'))
			*(cadena+strlen(cadena))= (char) evento.key.keysym.unicode;
		else if(evento.key.keysym.sym== SDLK_BACKSPACE)
			*(cadena+strlen(cadena)-1)= (char) '\0';
	    	else if(evento.key.keysym.sym== SDLK_RETURN)
	    		return 1;
    		*(cadena+strlen(cadena))= '\0';
		if(strlen(cadena)>15)
			return 1;
    	}
	return 0;
}

SDL_Color newColor(int r, int g, int b){
	SDL_Color color;
	color.r=r;
	color.g=g;
	color.b=b;
	return color;
}

void renderMainScreen(int opcion){
	SDL_Rect rectAux;
	SDL_Surface * texto;
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
	
	inicializarRectangulo(&rectAux, 39, 54, 0, 0);
	SDL_BlitSurface(plantilla1, &rectTitle, screen, &rectAux);

	generarMarco(100,250,300,150, newColor(255,255,0));
	
	inicializarRectangulo(&rectAux, 180, 285, 0, 0);
	if(opcion==1){
		SDL_Rect brick;
		inicializarRectangulo(&brick, 173, 0, 20, 16);
		SDL_BlitSurface(plantilla1, &brick,screen,&rectAux);
		escribirEnPantalla((char *)"Play", 215,285, font, newColor(0, 128,255));
	}
	else
		escribirEnPantalla((char *)"Play", 215,285, font, newColor(255,255,255));
	
	rectAux.y+= 50;
	if(opcion==2){
		SDL_Rect brick;
		inicializarRectangulo(&brick, 173, 0, 20, 16);
		SDL_BlitSurface(plantilla1, &brick,screen,&rectAux);
		escribirEnPantalla((char *)"Config", 215,335, font, newColor(0, 128,255));
	}
	else
		escribirEnPantalla((char *)"Config", 215,335, font, newColor(255, 255,255));
	
	escribirEnPantalla((char *)"SISTEMAS OPERATIVOS 2014", 85, 440, font, newColor(255, 0,0));
	escribirEnPantalla((char *)"BARRETT - CASABONA - SURIANO", 65, 480, font, newColor(255, 0,0));

	SDL_Flip(screen);
}

void renderOptionScreen(int opcion){
	char linea[25];
	char aux[20];
	SDL_Rect rectAux;

	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

	inicializarRectangulo(&rectAux, 39, 15, 0, 0);
	SDL_BlitSurface(plantilla1, &rectTitle, screen, &rectAux);

	generarMarco(20, 200, 466, 286, newColor(255, 255,0));

	escribirEnPantalla((char *)"Server Stats", 50,230, font, newColor(255,0,0));

	sprintf(linea, "Ip: %s", configuracion.IP);
	if(opcion==1)
		escribirEnPantalla(linea, 50,260, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 50,260, font, newColor(255,255,255));

	sprintf(linea, "Port: %d", configuracion.port);
	if(opcion==2)
		escribirEnPantalla(linea, 300,260, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 300,260, font, newColor(255,255,255));

	escribirEnPantalla((char *)"Controls", 50,300, font, newColor(255,0,0));

	mostrarTecla(configuracion.up, aux);
	sprintf(linea, "Up: %s", aux);
	if(opcion==3)
		escribirEnPantalla(linea, 50,330, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 50,330, font, newColor(255,255,255));

	mostrarTecla(configuracion.down, aux);
	sprintf(linea, "Down: %s", aux);
	if(opcion==4)
		escribirEnPantalla(linea, 250,330, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 250,330, font, newColor(255,255,255));

	mostrarTecla(configuracion.right, aux);
	sprintf(linea, "Right: %s", aux);
	if(opcion==5)
		escribirEnPantalla(linea, 50,360, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 50,360, font, newColor(255,255,255));

	mostrarTecla(configuracion.left, aux);
	sprintf(linea, "Left: %s", aux);
	if(opcion==6)
		escribirEnPantalla(linea, 250,360, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 250,360, font, newColor(255,255,255));

	mostrarTecla(configuracion.action, aux);
	sprintf(linea, "Fix: %s", aux);
	if(opcion==7)
		escribirEnPantalla(linea, 50,390, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 50,390, font, newColor(255,255,255));

	sprintf(linea, "Nick: %s", configuracion.nick);
	if(opcion==8)
		escribirEnPantalla(linea, 50,430, font, newColor(0,128,255));
	else
		escribirEnPantalla(linea, 50,430, font, newColor(255,255,255));

	escribirEnPantalla((char *)"ESC - Back", 40, 495, font, newColor(255,255,255));

	if(opcion==9)
		escribirEnPantalla((char *)"Save", 405, 495, font, newColor(255,255,0));
	else 
		escribirEnPantalla((char *)"Save", 405, 495, font, newColor(255,255,255));
	SDL_Flip(screen);
}

void loadAssetsMenu(){
	if(TTF_Init()!=0) {
		printf("Error al iniciar TTF: %s\n", TTF_GetError());
		exit (-1);
    }
	if((font = TTF_OpenFont("./ImagenesSDL/pixelmix.ttf", 20)) == NULL) {
		printf("Error al abrir fuente: %s\n", TTF_GetError());
		exit (-1);
	}
	cargarImagen(&plantilla1, "./ImagenesSDL/spritesFelix.png");
	inicializarRectangulo(&rectTitle, 0, 497, 428, 163); 
}

int cargarImagen(SDL_Surface ** imagen, const char * ruta){
	*imagen = IMG_Load(ruta);
	if (*imagen == NULL)
		return errorMessage("Error al cargar las imagenes!");
	return 0;
}

int compareRect(SDL_Rect * rect1, SDL_Rect * rect2){
	if(rect1->x == rect2->x && rect1->y == rect2->y)
		return 1;
	return 0;
}

void mostrarTecla(int valor, char * aux){
	switch(valor){
		case 273:
			strcpy(aux, "Up Ar.");
		break;
			// return (char *)"Up Arrow";
		case 274:
			strcpy(aux, "Down Ar.");
		break;
		case 275:
			strcpy(aux, "Right Ar.");
		break;
		case 276:
			strcpy(aux, "Left Ar.");
		break;
		case 32:
			strcpy(aux, "Space");
		break;
		default:
			sprintf(aux, "%c ", valor);
		break;
	}
}

void generarMarco(int x, int y, int tamX, int tamY, SDL_Color color){
	SDL_Rect rectAux;
	inicializarRectangulo(&rectAux, x, y, tamX, tamY);
	SDL_FillRect(screen, &rectAux, SDL_MapRGB(screen->format, 255, 255, 255));
	inicializarRectangulo(&rectAux, x+5, y+5, tamX-10, tamY-10);
	SDL_FillRect(screen, &rectAux, SDL_MapRGB(screen->format, 0, 0, 0));
	inicializarRectangulo(&rectAux, x+8, y+8, tamX-16, tamY-16);
	Uint32 map= colorToMap(color);
	SDL_FillRect(screen, &rectAux, map);
	inicializarRectangulo(&rectAux, x+13, y+13, tamX-26, tamY-26);
	SDL_FillRect(screen, &rectAux, SDL_MapRGB(screen->format, 0, 0, 0));
}

Uint32 colorToMap(SDL_Color color){
	return SDL_MapRGB(screen->format, color.r, color.g, color.b);
}

void inicializarRectangulo(SDL_Rect * rect, int x,int y,int w,int h){
	rect->x= x;
	rect->y= y;
	rect->w= w;
	rect->h= h;	
}

int handleEventMenu(SDL_Event evento, int * opcion, int render){
	switch (evento.type){
		case SDL_KEYDOWN:
			switch(evento.key.keysym.sym){
				case SDLK_UP:
					if(*opcion>1)
						*opcion-=1;
					if(render==0)
						renderMainScreen(*opcion);
					else
						renderOptionScreen(*opcion);
				break;
				case SDLK_DOWN:
					if(render==0){
						if(*opcion<2)
							*opcion +=1;
						renderMainScreen(*opcion);
					}
					else{
						if(*opcion<9)
							*opcion +=1;
						renderOptionScreen(*opcion);
					}
				break;
				case SDLK_RETURN:
					return action(*opcion, render);
				break;
				case SDLK_ESCAPE:
					return 2;
				break;
			}
		break;
		case SDL_QUIT:
			exit(0);
		break;
	}
	return 0;
}