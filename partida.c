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
#include <string.h>
#include <wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/shm.h>

#define TAMBUF 512
#define MAXQ 10
#define matrizX 11
#define matrizY 6

//estados de los jugadores
#define JUGANDO 0
#define DISPONIBLE 1
#define ESPERANDO 2
#define ABANDONO 3
#define TORTA 90

union semun{
	int val;
	struct semid_ds *buf;
	ushort * array;
}arg;

struct persona{  // struct d elos clientes
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

struct pareja{
	struct persona personaA;
	struct persona personaB;
	int sala;
	int matriz[matrizY][matrizX];
	int matVentanas[matrizY][matrizX];
	char emisor;
};

//THREADS
void * lanzador(void * args);
void * birdHandler(void * args);
void * brickHandler(void * args);
void * verificarPersona(void * args);
void * generarPajaros(void * args);
void * pierdeVidaPersona(void * args);
void * collisionListener(void * args);
void * modoBerserker(void *args);
void * stageListener(void * args);
void * generarTorta(void * args);
void limpiarMatriz(void * args);
void rellenarCadena(char * argumento);
void cerrarSocket();
void mostrarMatriz(int mat[][11]);
void nuevaPareja(struct pareja *pareja, struct persona *personaA, struct persona *personaB, int sala);
void mostrarDatosPareja(struct pareja *pareja);
int obtenerSemaforo(key_t clave,int cantsem) ;
int setiarUnSemaforo(int IdSemaforo,int numSem,int valor);
void pedirSemaforo(int IdSemaforo,int numSem, int cantOp) ;
void devolverSemaforo(int IdSemaforo,int numSem,int cantOp);
void eliminarSemaforo(int IdSemaforo);
void generarTechos(int matobjetos[][matrizX],char * buffer);
int generarVentanas(int matVentanas[][matrizX], char * buffer);
void generarMarcos(int matobjetos[][matrizX],char * buffer);
void mostrarDatosPersona(struct persona *pers);

int choqueLadrillo[3][2];
int ladrillos[3];

pthread_t pajaroA = 0; 
pthread_t pajaroB = 0; 
pthread_t pajaroC = 0;
int pos[2]={1,9};

int conect1=0;
int conect2=0;

int nivel=1;
int contStage=0;
int numPato = 1;
int tInmunidad;

int idShMem; //ID memoria compartida
int idSemReadShMem; //ID Semaforo para leer en la memoria compartida
int idSemWriteShMem; //ID Semaforo para escribir en la memoria compartida

int rotas;
int VIDAUP=2000;

int socketPersonaA;
int socketPersonaB;

int main(int argc, char *argv[]) {
	if(argc!=3){
		printf("No se puede Cargar la Partida\n");
		return 0;
	}
	printf("SE CREO LA SALA!\n");
	pthread_t threadF;
	signal(SIGINT, SIG_IGN);
	struct pareja pareja;
	int idSemChild;	
	int sala = atoi(argv[1]);
	tInmunidad= atoi(argv[2]);
	//Semaforos de la memoria compartida, es un mutex para leer/escribir
	key_t ShMemKey = ftok(".", 4000);
	idSemReadShMem = obtenerSemaforo(ShMemKey, 1);
	ShMemKey = ftok(".", 3000);
	idSemWriteShMem = obtenerSemaforo(ShMemKey, 1);	
	ShMemKey = ftok(".", 2000);
	idSemChild = obtenerSemaforo(ShMemKey, 1);
	//Memoria compartida
	key_t keyShMem = ftok(".", 5000);
	idShMem = shmget(keyShMem,sizeof(struct persona) * 2,IPC_CREAT | 0660);
	//Leo los datos de la memoria
	struct persona * memoria = shmat(idShMem, NULL, 0);  // Lee la memoria compartida y toma los Clientes
	struct persona personaA = memoria[0];
	struct persona personaB = memoria[1];
	devolverSemaforo(idSemChild, 0 ,1);
	devolverSemaforo(idSemWriteShMem, 0 ,1);

	nuevaPareja(&pareja, &personaA, &personaB, sala);
	pthread_create(&threadF,NULL,&stageListener, (void *) &pareja);
	pthread_join(threadF, NULL);
	//Guarda en la memoria compartida los puntos de cada cliente
	if(pareja.personaA.estado != ABANDONO)
		pareja.personaA.estado = DISPONIBLE;
	if(pareja.personaB.estado != ABANDONO)
		pareja.personaB.estado = DISPONIBLE;
	pareja.personaA.sala = 0;
	pareja.personaB.sala = 0;
	if(getppid()!=1){
		pedirSemaforo(idSemWriteShMem, 0 , -1);  // Cuando termina escribe los datos en la Memoria Compartida
		memoria = shmat(idShMem, NULL, 0);
		memoria[0] = pareja.personaA;
		memoria[1] = pareja.personaB;
		devolverSemaforo(idSemReadShMem, 0 ,1);
	}
	else{
		eliminarSemaforo(idSemReadShMem);  // Si se colgo el torneo Borra los semaforos y la memoria
		eliminarSemaforo(idSemWriteShMem);
		eliminarSemaforo(idSemChild);
		shmctl(idShMem, IPC_RMID, 0);
	}
	printf("Se abondono la sala!\n");
	close(socketPersonaA); //cierro el socket cliente 
	perror("close");
	close(socketPersonaB); //cierro el socket cliente 
	perror("close");
	return 0; //muerte del proceso hijo
}

void mostrarDatosPersona(struct persona *pers){  // muestra datos persona
	printf("--------------------------\n");
	printf("ID: %d\n", pers->id);
	printf("NOMBRE: %s\n", pers->nombre);
	printf("IP: %s\n", inet_ntoa(pers->ipPersona));
	printf("SOCKET: %d\n", pers->socket);
	printf("VIDAS: %d\n", pers->cantVida);
	printf("PUNTOS: %d\n", pers->puntos);
	printf("PUESTO: %d\n", pers->puesto+1);
	printf("--------------------------\n");
}

void nuevaPareja(struct pareja *pareja, struct persona *personaA, struct persona *personaB, int sala){   // Genera la Pareja de Clientes
	personaA->cantVida = 3; 
	personaB->cantVida = 3;
	personaA->puntos = 0;
	personaB->puntos = 0;
	char buffer[TAMBUF];
	pareja->personaA= *personaA;
	pareja->personaB= *personaB;
	pareja->sala= sala;
	pareja->emisor = 0;
	socketPersonaA = personaA->socket;
	socketPersonaB = personaB->socket;
	for (int i = 0; i < matrizY; i++)
		for (int j = 0; j < matrizX; j++)
			pareja->matriz[i][j]= 0;

	// Manda los nombre de los clientes
	strcpy(buffer,"25 "); 
	strcat(buffer,pareja->personaB.nombre);
	rellenarCadena(buffer);
	send(pareja->personaA.socket, buffer, TAMBUF, 0);

	strcpy(buffer,"25 ");
	strcat(buffer,pareja->personaA.nombre);
	rellenarCadena(buffer);
	send(pareja->personaB.socket, buffer, TAMBUF, 0);
}
void generarTechos(int matobjetos[][matrizX],char * buffer){  // Algoritmo para generar los techos
	char aux[3];
	int probXnivel=nivel+5; // cantidad de techos varia por la dificultad
	srand(time(NULL));
	int cantMar=10;
	int techos[cantMar];
	for (int i = 0; i < probXnivel; ++i)
	{
		int fil = (rand()%2+1)*2;
		int col = rand()%9+1;
		while( col % 2 == 0)
			col = rand()%9+1;
		if(col!=1&&col!=9){
			if(matobjetos[fil][col+2]!=100 && matobjetos[fil][col-2]!=100 
				&& matobjetos[fil+1][col+1]!=100 && matobjetos[fil+1][col-1]!=100 
				&& matobjetos[fil-1][col+1]!=100 && matobjetos[fil-1][col-1]!=100)
				matobjetos[fil][col] = 100;
		}
	}
	strcat(buffer,"53 ");
	for (int i = 2; i <= 4; i+=2) // escribo un vector del estado de los techos
	{
		for(int j=1; j<= 9; j+=2){ 
			sprintf(aux,"%d",matobjetos[i][j]/100); 
			strcat(buffer,aux); 
		}
	}
	rellenarCadena(buffer);
}

void generarMarcos(int matobjetos[][matrizX],char * buffer){  // algoritmo para generar los Marcos de las ventanas
	char aux[3];
	int probXnivel=3;
	srand(time(NULL));
	int cantMar=12;
	for (int i = 0; i < probXnivel; ++i){
		int fil = rand()%5+1;
		int col = (rand()%4+1)*2;
		while( fil % 2 == 0)
			fil = rand()%5+1;
			if(matobjetos[fil+2][col]!=100 && matobjetos[fil-2][col]!=100 
				&& matobjetos[fil+1][col+1]!=100 && matobjetos[fil+1][col-1]!=100 
				&& matobjetos[fil-1][col+1]!=100 && matobjetos[fil-1][col-1]!=100)
				matobjetos[fil][col] = 100;
	}
	strcat(buffer,"52 ");
	for (int i = 1; i <= 5; i+=2)
		for (int j = 2; j <= 8; j+=2){
		sprintf(aux,"%d",matobjetos[i][j]/100); 
		strcat(buffer,aux); 
		}
	rellenarCadena(buffer);
}

void * pierdeVidaPersona(void * args){  // Thread que se encarga de la Inmunidad dps de perder una Vida.
	struct persona* persona = args;
	persona->racha=0;
	persona->inmunidad=1;
	sleep(5);
	persona->inmunidad=0;
}

int generarVentanas(int matVentanas[][matrizX], char * buffer){ // Algoritmo para Generar las ventanas
	time_t tiempo; 
	char aux[3]; 
	int probrotas=nivel+6; 
	time(&tiempo); 
	srand(tiempo); 
	int roto; 
	int cont=0;
	int rotas = 0;
	int cantVentanas=15; 
	int ventanas[cantVentanas]; 
	for (int i = 0; i < matrizY; i++) 
		for (int j = 0; j < matrizX; j++) 
			matVentanas[i][j]= 0; 
	for (int i = 1; i < matrizY; i+=2){ 
		for (int j = 1; j < matrizX; j+=2){ 
			roto=rand()%10; 
			if(roto<probrotas){ 
				rotas++;
				int ventana = rand()%3; 
				if(ventana == 0) matVentanas[i][j] = 51; // rota 1, arriba
				if(ventana == 1) matVentanas[i][j] = 52; // rota 1, abajo
				if(ventana == 2) matVentanas[i][j] = 53; // rota las 2
			} 
			else
				matVentanas[i][j] = 50; 	// ventana entera
		ventanas[cont]=matVentanas[i][j]; 
		cont++; 
		} 
	} 
	strcat(buffer,"51 ");  // valor para indicar que el mensaje es del estado de las ventanas
	for(int i=0; i< cont;i++){   // genero el vector del estado de las ventanas
		sprintf(aux,"%d",ventanas[i]-50); 
		strcat(buffer,aux); 
	}
	rellenarCadena(buffer);
	return rotas;
}

void mostrarDatosPareja(struct pareja *pareja){  // Muestra datos de la pareja
	fflush(stdout);
	printf("--------------------------\n");
	printf("Sala: %d\n", pareja->sala);
	printf("IpPersonaA: %s\n", inet_ntoa(pareja->personaA.ipPersona));
	printf("IDPersonaA: %d\n", pareja->personaA.id);
	printf("IpPersonaB: %s\n", inet_ntoa(pareja->personaB.ipPersona));
	printf("IDPersonaB: %d\n", pareja->personaB.id);
	printf("--------------------------\n");
}

// Thread Principal del programa. Se encarga de lanzar las otras etapas como Verificadores de los clientes y los threads de los objetos
void * stageListener(void * args){  
	char buffer[TAMBUF];
	struct pareja * pareja = args;
	pthread_t threadPajaros;
	pthread_t threadBrick;
	pthread_t threadCollision;
	pthread_t threadTorta;
	pthread_t threadA;
	pthread_t threadB;
	while((pareja->personaA.cantVida>0 || pareja->personaB.cantVida>0)&&(pareja->personaA.socket!=pareja->personaB.socket)){
		while(contStage<nivel+2 && (pareja->personaA.socket!=pareja->personaB.socket) ){
			///////////////////////////////////////////////////////
			limpiarMatriz((void *)pareja);
			pthread_create(&threadA,NULL,&verificarPersona, (void *) pareja); //definimos el thread que escucha al cliente 1
			usleep(10000);
			pthread_create(&threadB,NULL,&verificarPersona, (void *) pareja); //definimos el thread que escucha al cliente 2
			bzero(buffer, TAMBUF);
			rotas = generarVentanas(pareja->matVentanas, buffer); // indica la cantidad de ventanas rotas

			send(pareja->personaA.socket, buffer, TAMBUF, 0);
			send(pareja->personaB.socket, buffer, TAMBUF, 0);
			if(contStage != 0){
				usleep(100000);
				bzero(buffer, TAMBUF);
				generarMarcos(pareja->matVentanas, buffer);

				send(pareja->personaA.socket, buffer, TAMBUF, 0);
				send(pareja->personaB.socket, buffer, TAMBUF, 0);
				usleep(100000);
				bzero(buffer, TAMBUF);
				generarTechos(pareja->matVentanas, buffer);

				send(pareja->personaA.socket, buffer, TAMBUF, 0);
				send(pareja->personaB.socket, buffer, TAMBUF, 0);
				pthread_create(&threadTorta,NULL,&generarTorta,(void *) pareja);
			} else {
				strcpy(buffer, "52 000000000000 ---");
				send(pareja->personaA.socket, buffer, TAMBUF, 0);
				send(pareja->personaB.socket, buffer, TAMBUF, 0);
				usleep(100000);
				strcpy(buffer, "53 0000000000 -----");
				send(pareja->personaA.socket, buffer, TAMBUF, 0);
				send(pareja->personaB.socket, buffer, TAMBUF, 0);
				usleep(100000);
			}
			numPato=1;
			pthread_create(&threadPajaros,NULL,&lanzador,(void *) pareja);			
			pthread_create(&threadBrick,NULL,&brickHandler,(void *) pareja);
			pthread_create(&threadCollision,NULL,&collisionListener, (void *) pareja);
			do{
				usleep(500000);
			}while(rotas!=0 && pareja->personaA.socket!=pareja->personaB.socket );

			pthread_cancel(threadA);
			pthread_cancel(threadB);
			pthread_cancel(threadCollision);
			if(contStage!=0) pthread_cancel(threadTorta);
			pthread_cancel(threadBrick);
			pthread_cancel(threadPajaros);

			if(pajaroA!=0)	pthread_cancel(pajaroA);
			if(pajaroB!=0)	pthread_cancel(pajaroB);
			if(pajaroC!=0)	pthread_cancel(pajaroC);
			limpiarMatriz((void *)pareja);

			// libero recursos de los threads cancelados
			pthread_join(threadA,NULL);
			pthread_join(threadB,NULL);
			pthread_join(threadCollision,NULL);
			pthread_join(threadBrick,NULL);
			pthread_join(threadPajaros,NULL);

			// seteo el id de los thread en 0
			pajaroA=0;
			pajaroB=0;
			pajaroC=0;

			limpiarMatriz((void *)pareja); // limpia la matriz de objetos

			if(contStage+1!=nivel+2){
				strcpy(buffer,"30"); // paso de Stage
			}
			else{
				strcpy(buffer,"10"); // paso de nivel
			}
			rellenarCadena(buffer);
			send(pareja->personaA.socket, buffer, TAMBUF, 0);
			send(pareja->personaB.socket, buffer, TAMBUF, 0);

			contStage++;
			pareja->emisor=0;
		}
	contStage=0;
	VIDAUP+=500;
	nivel++;
	}
}
void limpiarMatriz(void * args){  // Limpia la matriz de objetos
	struct pareja * pareja = args;
		for (int i = 0; i < matrizY; i++){
			for (int j = 0; j < matrizX; j++){
					pareja->matriz[i][j]=0;
					pareja->matVentanas[i][j]=0;
			}
	}
}

void * modoBerserker(void *args){  // thread para Setiar El modo Berserker
	struct persona* persona = args;
	persona->inmunidad=1;
	persona->berserker=1;
	sleep(tInmunidad);
	persona->inmunidad=0;
	persona->berserker=0;
}

void * generarTorta(void * args){  // Algoritmo para Generar Tortas en las Stages
	pthread_attr_t attr; // thread attribute
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); // Para hacer el thread del modo berserker del tipo detached

	struct pareja * pareja = args;
	srand(time(NULL));
	int fil;
	int col;
	char buffer[TAMBUF];
	int piso = contStage;
	sleep(7+(nivel*2));
	if(piso ==contStage){
    	// BUSCA LUGAR DONDE MANDAR LA TORTA
		col = rand()%10;
		fil = rand()%6;
		do{
			col=2;
			fil=2;
			while( col % 2 == 0)
				col = rand()%10;
			while( fil % 2 == 0)
				fil = rand()%6;
		}while(pareja->matVentanas[fil][col]!=50 && pareja->matVentanas[fil][col]!=0 && pareja->matVentanas[fil][col]!=53);
		//

		sprintf(buffer, "%d %d %d", TORTA,fil,col);
		rellenarCadena(buffer);
		send(pareja->personaB.socket, buffer, TAMBUF, 0);
		send(pareja->personaA.socket, buffer, TAMBUF, 0);

		sleep(4);//ANIMATOR. Tiempo para q salga la torta

		 // sentencias para verificar si la torta aparecio en un lugar donde esta parado un player
		
		if(pareja->matriz[fil][col]==1 || pareja->matriz[fil][col]==5 ||pareja->matriz[fil][col]==9||pareja->matriz[fil][col]==13){  // jugador 1 parado donde salio la torta
			sprintf(buffer, "1 %d %d %d",TORTA ,pareja->personaA.cantVida, tInmunidad);
			pthread_t threadAux;
			pthread_create(&threadAux,&attr,&modoBerserker, (void *) &pareja->personaA);

			
			sprintf(buffer, "1 %d %d", TORTA,tInmunidad);
			rellenarCadena(buffer);
			send(pareja->personaA.socket, buffer, TAMBUF, 0);
			

			sprintf(buffer, "2 %d %d",TORTA , tInmunidad);
			rellenarCadena(buffer);
			send(pareja->personaB.socket, buffer, TAMBUF, 0);

		}
		else
		if(pareja->matriz[fil][col]==2 || pareja->matriz[fil][col]==6 ||pareja->matriz[fil][col]==10||pareja->matriz[fil][col]==14){  // jugador 2 estaba parado donde salio la torta
			sprintf(buffer, "1 %d %d %d",TORTA ,pareja->personaB.cantVida, tInmunidad);
			pthread_t threadAux;
			pthread_create(&threadAux,&attr,&modoBerserker, (void *) &pareja->personaB);

			sprintf(buffer, "1 %d %d ", TORTA,tInmunidad);
			rellenarCadena(buffer);
			send(pareja->personaB.socket, buffer, TAMBUF, 0);
			
			sprintf(buffer, "2 %d %d",TORTA, tInmunidad);
			rellenarCadena(buffer);
			send(pareja->personaA.socket, buffer, TAMBUF, 0);

		}
		else
		if(pareja->matriz[fil][col]==3 || pareja->matriz[fil][col]==7 ||pareja->matriz[fil][col]==11||pareja->matriz[fil][col]==15){  // jugador 1 y 2 parados donde salio la torta
			int comio = rand()%2+1;
			if(comio==1){

				pthread_t threadAux;
				pthread_create(&threadAux,&attr,&modoBerserker, (void *) &pareja->personaA);
				
				sprintf(buffer, "1 %d %d ", TORTA,tInmunidad);
				rellenarCadena(buffer);
				send(pareja->personaA.socket, buffer, TAMBUF, 0);
				
				sprintf(buffer, "2 %d %d",TORTA, tInmunidad);
				rellenarCadena(buffer);
				send(pareja->personaB.socket, buffer, TAMBUF, 0);
			}
			else{

				pthread_t threadAux;
				pthread_create(&threadAux,&attr,&modoBerserker, (void *) &pareja->personaB);

				sprintf(buffer, "1 %d %d ", TORTA,tInmunidad);
				rellenarCadena(buffer);
				send(pareja->personaB.socket, buffer, TAMBUF, 0);
			
				sprintf(buffer, "2 %d %d",TORTA, tInmunidad);
				rellenarCadena(buffer);
				send(pareja->personaA.socket, buffer, TAMBUF, 0);
				
			}
		}
		else
			pareja->matVentanas[fil][col]+=10; // valor de torta en la matriz. SI no la comieron de entrada

	}
}

void * collisionListener(void * args){  // Thread que verifica las colisiones con los Objetos
	struct pareja * pareja = args;
	char buffer[TAMBUF];
	int con=0;
	int flagA, flagB;
	while(rotas!=0 && (pareja->personaA.socket!=pareja->personaB.socket)){
		usleep(10000);
		flagA=0;
		flagB=0;
		con++;
		int n;
		for(int i = 0; i < matrizY; i++)
			for(int j = 0; j < matrizX; j++){
				n=0;
				switch(pareja->matriz[i][j]){
					case 5: //Paloma a A
						if(pareja->personaA.inmunidad==0 && pareja->personaA.cantVida > 0){  // choca si no esta en modo inmune y si ademas tiene vidas.
							pthread_t threadAux;
							pareja->personaA.cantVida --; //Si choco se resta una vida
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaA);
							flagA=1;
							//printf("Persona A perdio una vida\n");
						}
					break;
					case 6: //Paloma a B
						if(pareja->personaB.inmunidad==0 && pareja->personaB.cantVida > 0){ // choca si no esta en modo inmune y si ademas tiene vidas.
							pthread_t threadAux;
							pareja->personaB.cantVida --; 
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaB);
							flagB=1;
							//printf("Persona B perdio una vida\n");
						}
					break;
					case 7: //Paloma a A y B
						if(pareja->personaA.inmunidad==0 && pareja->personaA.cantVida > 0){ // choca si no esta en modo inmune y si ademas tiene vidas.
							pthread_t threadAux;
							pareja->personaA.cantVida --; //Si choco se resta una vida
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaA);
							flagA=1;
							//printf("Persona A perdio una vida\n");
						}
						if(pareja->personaB.inmunidad==0 && pareja->personaB.cantVida > 0){ // choca si no esta en modo inmune y si ademas tiene vidas.
							pthread_t threadAux;
							pareja->personaB.cantVida --; 
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaB);
							flagB=1;
							//printf("Persona B perdio una vida\n");
						}
					break;
					case 9: //Ladrillo a A
						if(pareja->personaA.inmunidad==0 && pareja->personaA.cantVida > 0){ // choca si no esta en modo inmune y si ademas tiene vidas.
							for (int k = 0; k < 3; ++k) // para detectar con que ladrillo choco.
								if(choqueLadrillo[k][0]==j && choqueLadrillo[k][1]==i){
									n=k+1;
									ladrillos[k]=1;
								}

							pthread_t threadAux;
							pareja->personaA.cantVida --; //Si choco se resta una vida
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaA);
							flagA=1;
							//printf("Persona A perdio una vida\n");
						}
					break;
					case 10: //Ladrillo a B
						if(pareja->personaB.inmunidad==0 && pareja->personaB.cantVida > 0){ // choca si no esta en modo inmune y si ademas tiene vidas.
							for (int k = 0; k < 3; ++k)  // para detectar con que ladrillo choco.
								if(choqueLadrillo[k][0]==j && choqueLadrillo[k][1]==i){
									n=k+1;
									ladrillos[k]=1;
								}

							pthread_t threadAux;
							pareja->personaB.cantVida --;  //Si choco se resta una vida
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaB);
							flagB=1;
						}
					break;
					case 11: //Ladrillo a A y B
						if(pareja->personaA.inmunidad==0 && pareja->personaA.cantVida > 0){  // choca si no esta en modo inmune y si ademas tiene vidas.
							for (int k = 0; k < 3; ++k) // para detectar con que ladrillo choco.
								if(choqueLadrillo[k][0]==i && choqueLadrillo[k][1]==j){
									n=k+1;
									ladrillos[k]=1;
								}

							pthread_t threadAux;
							pareja->personaA.cantVida --; //Si choco se resta una vida
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaA);
							flagA=1;
						}
						if(pareja->personaB.inmunidad==0 && pareja->personaB.cantVida > 0){  // choca si no esta en modo inmune y si ademas tiene vidas.
							for (int k = 0; k < 3; ++k)
								if(choqueLadrillo[k][0]==i && choqueLadrillo[k][1]==j){
									n=k+1;
									ladrillos[k]=1;
								}

							pthread_t threadAux;
							pareja->personaB.cantVida --; 
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaB);
							flagB=1;
						}
					break;
					case 13: //Paloma y Ladrillo a A
						if(pareja->personaA.inmunidad==0 && pareja->personaA.cantVida > 0){
							for (int k = 0; k < 3; ++k)
								if(choqueLadrillo[k][0]==i && choqueLadrillo[k][1]==j){
									n=k+1;
									ladrillos[k]=1;
								}
							pthread_t threadAux;
							pareja->personaA.cantVida --;  //Si choco se resta una vida
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaA);
							flagA=1;
						}
					break;
					case 14: //Paloma y Ladrillo B
						if(pareja->personaB.inmunidad==0 && pareja->personaB.cantVida > 0){
							for (int k = 0; k < 3; ++k)
								if(choqueLadrillo[k][0]==i && choqueLadrillo[k][1]==j){
									n=k+1;
									ladrillos[k]=1;
								}
							pthread_t threadAux;
							pareja->personaB.cantVida --; //Si choco se resta una vida
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaB);
							flagB=1;
						}
					break;
					case 15: //Paloma y Ladrillo a A y B
						if(pareja->personaA.inmunidad==0 && pareja->personaA.cantVida > 0){
							for (int k = 0; k < 3; ++k)
								if(choqueLadrillo[k][0]==i && choqueLadrillo[k][1]==j){
									n=k+1;
									ladrillos[k]=1;
								}
							pthread_t threadAux;
							pareja->personaA.cantVida --; //se hace aca por las dudas que el thread no llegue a cambiar el estado de la variable
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaA);
							flagA=1;
						}
						if(pareja->personaB.inmunidad==0 && pareja->personaB.cantVida > 0){
							for (int k = 0; k < 3; ++k)
								if(choqueLadrillo[k][0]==i && choqueLadrillo[k][1]==j){
									n=k+1;
									ladrillos[k]=1;
								}
							pthread_t threadAux;
							pareja->personaB.cantVida --; 
							pthread_create(&threadAux,NULL,&pierdeVidaPersona, (void *) &pareja->personaB);
							flagB=1;
							//printf("Persona B perdio una vida\n");
						}
					break;
				}
				if(flagA==1){					
					sprintf(buffer, "1 80 %d 5 %d", pareja->personaA.cantVida,n); // n!=0 ladrillo que choco, n = 0 pato, 5 tiempo inmunidad, //80 vida Cambio

					rellenarCadena(buffer);
					send(pareja->personaA.socket, buffer, TAMBUF, 0);			

					sprintf(buffer, "2 80 %d 5 %d", pareja->personaA.cantVida,n);
					rellenarCadena(buffer);
					send(pareja->personaB.socket, buffer, TAMBUF, 0);

					flagA=0;
				}
				if(flagB==1){
					sprintf(buffer, "1 80 %d 5 %d", pareja->personaB.cantVida,n);

					rellenarCadena(buffer);
					send(pareja->personaB.socket, buffer, TAMBUF, 0);

					sprintf(buffer, "2 80 %d 5 %d", pareja->personaB.cantVida,n);
					rellenarCadena(buffer);
					send(pareja->personaA.socket, buffer, TAMBUF, 0);

					flagB=0;
				}

			}
	}
}


void mostrarMatriz(int mat[][11]){
	for (int i = 0; i < matrizY; i++){
		for (int j = 0; j < matrizX; j++)
				printf("%d ", mat[i][j]);
				printf("0 ");

		printf(" \n");
	}
	puts("---------------");
}

void * brickHandler(void * args){ // manejador de los ladrillos
struct pareja * pareja =args;
sleep(3);
int retardo=0;
int posant=5;
int lad1=0,lad2=0,lad3=0;
int tiemxCuad=1000000;
int tladrillo=2000000; 

if(nivel>=3){
	tladrillo=1000000;
	tiemxCuad=500000;
}
char buffer[TAMBUF]; 
int i,mov1,mov2; 
while(rotas!=0 && (pareja->personaA.socket!=pareja->personaB.socket)){ 
	//sleep(2); 
	srand(time(NULL)); 
	i=rand()%(matrizX-2); 
	if(i%2==0) 
		i++; // Si da par le sumo 1 
	if(i<=posant) 
		retardo=(posant-i); 
	else if(i>posant)
		retardo=(i-posant);

	// Desplazamiento de los ladrillos respecto al primero
	mov1=(rand()%2)*2; // desplazamiento con respecto al anterior 
	mov2=(rand()%2)*2; // desplazamiento con respecto al anterior 
	if(rand()%2==0 && i!=1) 
		mov1*=(-1); 
	else if(i==9) 
		mov1=0; 
	if(rand()%2==0 && i!=1) 
		mov2*=(-1); //RALPH 

	// Mover al RALPH
	sprintf(buffer,"20 %d %d",i,(tiemxCuad*retardo)); // ralph + pos + tiempo
	rellenarCadena(buffer);
	send(pareja->personaA.socket, buffer, TAMBUF, 0); 
	send(pareja->personaB.socket, buffer, TAMBUF, 0); 
	usleep(tiemxCuad*retardo);

	for (int i = 0; i < 3; ++i){
		for (int j = 0; j < 2; ++j)
			choqueLadrillo[i][j]=0;
		ladrillos[i]=0;
	}
	for (int j = 1; j <= matrizY+6 && (pareja->personaA.socket!=pareja->personaB.socket) && rotas != 0; j+=2){
			
			if(j<6 && ladrillos[0] == 0){
			 	lad1=1;
			 choqueLadrillo[0][0]=i;
			 choqueLadrillo[0][1]=j;
			 pareja->matriz[j][i]+=8; 
				 if(j==1){
				 	sprintf(buffer,"8 %d %d 1 %d",i,j,(int)((tladrillo/1000)*2.5)); 
				 	rellenarCadena(buffer);
			 		send(pareja->personaA.socket, buffer, TAMBUF, 0); 
			 		send(pareja->personaB.socket, buffer, TAMBUF, 0); 
				 }
			}
			if(j>2&& ladrillos[1] == 0 && j < 8){
				lad2=1;
				choqueLadrillo[1][0]=i+mov1;
			 	choqueLadrillo[1][1]=j-2;
			 	pareja->matriz[j-2][i+mov1]+=8;
			 	if(j==3){
			 		sprintf(buffer,"8 %d %d 2 %d",(i+mov1),(j-2),(int)((tladrillo/1000)*2.5));
			 		rellenarCadena(buffer);
					
			 	   		send(pareja->personaA.socket, buffer, TAMBUF, 0); 
			 	   	
			 	   		send(pareja->personaB.socket, buffer, TAMBUF, 0); 
			 	} 
			 }
			 if(j>4 && ladrillos[2] == 0 && j < 10){ 
			 	lad3=1;
			 	choqueLadrillo[2][0]=i+mov2;
			 	choqueLadrillo[2][1]=j-4;
			 	pareja->matriz[j-4][i+mov2]+=8; 
			 	if(j==5){ 
			 		sprintf(buffer,"8 %d %d 3 %d",(i+mov2),(j-4),(int)((tladrillo/1000)*2.5)); 
					rellenarCadena(buffer);
			 		
			 			send(pareja->personaA.socket, buffer, TAMBUF, 0); 
			 		
			 			send(pareja->personaB.socket, buffer, TAMBUF, 0); 
			 	} 
			}
			usleep(tladrillo*0.7);
			// SACARLO DE LA MATRIZ
			if((j<6 && ladrillos[0] == 0) || lad1==1){
				pareja->matriz[j][i]-=8; 
				lad1=0;
			}
			if((j>2&&j<8 && ladrillos[1] == 0) || lad2==1){
		   		pareja->matriz[j-2][i+mov1]-=8; 
		   		lad2=0;
			}
			if((j>4&&j<10 && ladrillos[2] == 0) || lad3==1){ 
		   		pareja->matriz[j-4][i+mov2]-=8;
		   		lad3=0; 
		   	}

		   	// los borra de un lado y los agrega en las pode contiguna intermedia entre ventanas.
		   	if(j<6 && ladrillos[0] == 0){
			 	lad1=1;
				 choqueLadrillo[0][0]=i;
				 choqueLadrillo[0][1]=j+1;
				 pareja->matriz[j+1][i]+=8; 

			}
			if(j>2&& ladrillos[1] == 0 && j < 8){
				lad2=1;
				choqueLadrillo[1][0]=i+mov1;
			 	choqueLadrillo[1][1]=j-2+1;
			 	pareja->matriz[j-2+1][i+mov1]+=8;

			 }
			 if(j>4 && ladrillos[2] == 0 && j < 10){ 
			 	lad3=1;
			 	choqueLadrillo[2][0]=i+mov2;
			 	choqueLadrillo[2][1]=j-4+1;
			 	pareja->matriz[j-4+1][i+mov2]+=8; 
			}

			usleep(tladrillo*0.3);

			if((j<6 && ladrillos[0] == 0) || lad1==1){
				pareja->matriz[j+1][i]-=8; 
				lad1=0;
			}
			if((j>2&&j<8 && ladrillos[1] == 0) || lad2==1){
		   		pareja->matriz[j-2+1][i+mov1]-=8; 
		   		lad2=0;
			}
			if((j>4&&j<10 && ladrillos[2] == 0) || lad3==1){ 
		   		pareja->matriz[j-4+1][i+mov2]-=8;
		   		lad3=0; 
		   	}
		   	// Los borra de las posicion intermedia.

		   	if(rotas == 0)
		   		break;
		} 
	posant=i; 
	} 
}

void * birdHandler(void * args){ // Thread que maneja los patos
	time_t tiempo;
	struct pareja * pareja =args;
	char buffer[TAMBUF];
	int j;
	int i;
	int inc;
	char lado;
	char limite;
	int tiempoSleep=2;
	if(nivel>=3)
		tiempoSleep=1;
	int frecPatos;
	if(frecPatos=2000000-(nivel*500000)>0)
		frecPatos=2000000-(nivel*500000);
	int numeroPato = numPato;
	while(rotas!=0 && (pareja->personaA.socket!=pareja->personaB.socket)){
		
		usleep(frecPatos);
		time(&tiempo);
		srand(tiempo);
		j=rand()%6;
		if(j%2==1){ 
			limite = -1; 
			i=matrizX-2; //en donde empieza el pato (9)
			inc=-2;
			lado='R';
		}
		else{ 
			j++;
			limite = matrizX; //(11) 
			i=1;
			inc=2;
			lado='L';
		}
		sprintf(buffer,"4 %d %d %c %d",j,(tiempoSleep*6),lado, numeroPato);
		rellenarCadena(buffer);
		send(pareja->personaA.socket, buffer, TAMBUF, 0);
		rellenarCadena(buffer);
		send(pareja->personaB.socket, buffer, TAMBUF, 0);
		if(rotas==0) break;
		if(pareja->personaB.socket!=pareja->personaA.socket)
			sleep(tiempoSleep);
		
		// mover pato dentro de la matriz de objetos
		while(i != limite && rotas!=0 && pareja->personaB.socket!=pareja->personaA.socket){
			pareja->matriz[j][i]+=4;
			usleep((tiempoSleep*1000000)); //valor tiempo sleep

			pareja->matriz[j][i]-=4;			
			i+=inc;

		}
		if(pareja->personaB.socket!=pareja->personaA.socket)
			sleep(tiempoSleep);
	}
}

void * lanzador(void * args){  // lanza los patos
	sleep(3);
	int tsleep=4;
	if(nivel>=3)
		tsleep=3;
	numPato=1;
	struct pareja * pareja = args; 
	pthread_create(&pajaroA,NULL,&birdHandler, (void *) pareja); 
	sleep(tsleep); 
	numPato++; 
	if(nivel>1 && rotas != 0){ 
		pthread_create(&pajaroB,NULL,&birdHandler, (void *) pareja); 
		sleep(tsleep); 
		numPato++; 
	} 
	if(nivel>2 && rotas != 0){ 
		pthread_create(&pajaroC,NULL,&birdHandler, (void *) pareja); 
		sleep(tsleep); 
		pthread_join(pajaroC,NULL); 
	}else{
			if(nivel>1 && rotas != 0)
				pthread_join(pajaroB,NULL); 
	}
	pthread_join(pajaroA,NULL); 
	numPato=1; 
}

void * verificarPersona(void * args){ 
	char buffer[TAMBUF];
	struct pareja *pareja = args;
	struct persona *emisor;
	struct persona *receptor;
	int i = 5;
	int desconectado=0;
	int j;
	int v;
	pthread_attr_t attr; // thread attribute
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(pareja->emisor == 0){
		pareja->emisor++;
		emisor = &(pareja->personaA);
		receptor = &(pareja->personaB);
		j = pos[0];
		v = 1;
	}
	else{
		emisor = &(pareja->personaB);
		receptor = &(pareja->personaA);
		j = pos[1];
		v = 2;
	}
	/////////////////////////////////////////////////////
	pareja->matriz[i][j] += v;
	sprintf(buffer, "1 %d %d", j, i);
	rellenarCadena(buffer);
	send(emisor->socket, buffer, TAMBUF, 0); //le mando la posicion inicial
	sprintf(buffer, "2 %d %d", j, i);
	rellenarCadena(buffer);
	send(receptor->socket, buffer, TAMBUF, 0); //le mando la posicion inicial
	////////////////////////////////////////////////////

	while(emisor->cantVida > 0 || receptor->cantVida > 0){
		bzero(buffer, TAMBUF); //rellena con ceros el resto de la estructura
		if(recv(emisor->socket, buffer, TAMBUF, 0) <= 0){
				desconectado=1;
				strcpy(buffer,"EXIT");
		}
		if(pareja->matriz[i][j]>0)pareja->matriz[i][j]-= v;
		if(strcmp(buffer, "EXIT")==0){
			emisor->estado = ABANDONO;
			emisor->cantVida = 0;
			strcpy(buffer, "2 EXIT");
			
			send(receptor->socket, buffer, TAMBUF, 0); //entonces le mando el mensaje
			if(getppid()!=1){
				pedirSemaforo(idSemWriteShMem, 0 , -1);
				struct persona *memoria = shmat(idShMem, NULL, 0);
				memoria[0] = *emisor;
				memoria[1] = *receptor;
				devolverSemaforo(idSemReadShMem, 0 ,1);
			}
			break;
		}
		if(emisor->cantVida > 0){ //Acciones que el jugador puede hacer si esta vivo
			if(strcmp(buffer, "UP")==0 && i > 1)
				if(pareja->matVentanas[i-1][j]<100){
					i--;
					pareja->matriz[i][j] += v;
					usleep(50000);
					pareja->matriz[i][j] -= v;
					i--;
					if (pareja->matVentanas[i][j]>=60)
					{
						pareja->matVentanas[i][j]-=10;
						
						pthread_t threadAux;
						pthread_create(&threadAux,&attr,&modoBerserker, (void *) emisor);

						sprintf(buffer, "1 %d %d ", TORTA,tInmunidad);
						rellenarCadena(buffer);
						send(emisor->socket, buffer, TAMBUF, 0);			


							sprintf(buffer, "2 %d %d",TORTA, tInmunidad);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);

					}
				}
			if(strcmp(buffer, "DOWN")==0 && i < 5)
				if(pareja->matVentanas[i+1][j]<100){
					i++;
					pareja->matriz[i][j] += v;
					usleep(50000);
					pareja->matriz[i][j] -= v;
					i++;
					if (pareja->matVentanas[i][j]>=60)
					{
						pareja->matVentanas[i][j]-=10;
						
						pthread_t threadAux;
						pthread_create(&threadAux,&attr,&modoBerserker, (void *) emisor);
						sprintf(buffer, "1 %d %d ", TORTA,tInmunidad);
						rellenarCadena(buffer);
						send(emisor->socket, buffer, TAMBUF, 0);	


							sprintf(buffer, "2 %d %d",TORTA, tInmunidad);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);

					}
				}
			if(strcmp(buffer, "LEFT")==0 && j > 2)
				if(pareja->matVentanas[i][j-1]<100){
					j-=2;
					if (pareja->matVentanas[i][j]>=60)
					{
						pareja->matVentanas[i][j]-=10;

						pthread_t threadAux;
						pthread_create(&threadAux,&attr,&modoBerserker, (void *) emisor);
						sprintf(buffer, "1 %d %d ", TORTA,tInmunidad);
						rellenarCadena(buffer);
						send(emisor->socket, buffer, TAMBUF, 0);	


							sprintf(buffer, "2 %d %d",TORTA, tInmunidad);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);
						
					}
				}
			if(strcmp(buffer, "RIGHT")==0 && j < 9)
				if(pareja->matVentanas[i][j+1]<100){
					j+=2;
					if (pareja->matVentanas[i][j]>=60)
					{
						pareja->matVentanas[i][j]-=10;
					
						pthread_t threadAux;
						pthread_create(&threadAux,&attr,&modoBerserker, (void *) emisor);

						sprintf(buffer, "1 %d %d ", TORTA,tInmunidad);
						rellenarCadena(buffer);
						send(emisor->socket, buffer, TAMBUF, 0);
				
						
							sprintf(buffer, "2 %d %d",TORTA, tInmunidad);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);
						
					}
				}
			pos[v-1]=j;
			if(strcmp(buffer, "HAMMER")==0){ 
				//printf("RECIBE HAMMER\n");
				int estado;
				switch(pareja->matVentanas[i][j]){
					case 51:
						emisor->puntos+= 100; 
						emisor->racha+=100;
						pareja->matVentanas[i][j]=50;

						if(emisor->racha==VIDAUP){
							emisor->racha=0;
							if(emisor->cantVida<3)
								emisor->cantVida++;
							sprintf(buffer,"1 80 %d 0 0",emisor->cantVida);// VER SI ANDA (80 - VIDA)
							rellenarCadena(buffer);
							send(emisor->socket, buffer, TAMBUF, 0);
							sprintf(buffer,"2 80 %d 0 0",emisor->cantVida);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);
						}
						sprintf(buffer, "50 %d %d 0 1 %d", j, i, emisor->puntos);
						rellenarCadena(buffer);
						send(emisor->socket, buffer, TAMBUF, 0);
						sprintf(buffer, "50 %d %d 0 2 %d", j, i, emisor->puntos);
						rellenarCadena(buffer);
						send(receptor->socket, buffer, TAMBUF, 0);
						rotas--;
					break;
					case 52:		
						emisor->puntos+= 100;
						emisor->racha+=100;

						if(emisor->racha==VIDAUP){
							emisor->racha=0;
							if(emisor->cantVida<3)
								emisor->cantVida++;
							sprintf(buffer,"1 80 %d 0 0",emisor->cantVida);// VER SI ANDA
							rellenarCadena(buffer);
							send(emisor->socket, buffer, TAMBUF, 0);
							sprintf(buffer,"2 80 %d 0 0",emisor->cantVida);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);
						}

						pareja->matVentanas[i][j]=50;
						sprintf(buffer, "50 %d %d 0 1 %d", j, i, emisor->puntos);
						rellenarCadena(buffer);
						send(emisor->socket, buffer, TAMBUF, 0);
						sprintf(buffer, "50 %d %d 0 2 %d", j, i, emisor->puntos);
						rellenarCadena(buffer);
						send(receptor->socket, buffer, TAMBUF, 0);
						rotas--;
						
					break;
					case 53:
						emisor->puntos+= 100;
						emisor->racha+=100;

						if(emisor->racha==VIDAUP){
							emisor->racha=0;
							if(emisor->cantVida<3)
								emisor->cantVida++;
							sprintf(buffer,"1 80 %d 0 0",emisor->cantVida);// VER SI ANDA
							rellenarCadena(buffer);
							send(emisor->socket, buffer, TAMBUF, 0);
							sprintf(buffer,"2 80 %d 0 0",emisor->cantVida);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);
						}

						if(emisor->berserker == 1){
							rotas--;
							pareja->matVentanas[i][j]=50;
							estado = 0;
						}
						else{
							pareja->matVentanas[i][j] --;
							estado = 2;
						}
							sprintf(buffer, "50 %d %d %d 1 %d", j, i, estado,emisor->puntos); // 4 parametro estado
							rellenarCadena(buffer);
							send(emisor->socket, buffer, TAMBUF, 0);
							sprintf(buffer, "50 %d %d %d 2 %d", j, i, estado,emisor->puntos);
							rellenarCadena(buffer);
							send(receptor->socket, buffer, TAMBUF, 0);
						
					break;
				}
				strcpy(buffer, "1 300");
			}
			else
				sprintf(buffer, "1 %d %d", j, i);
			
			rellenarCadena(buffer);
			send(emisor->socket, buffer, TAMBUF, 0); //envias lo que esta cargado en el buffer
				if(strstr(buffer,"1 300")!=NULL)
					strcpy(buffer, "2 300");
				else
					sprintf(buffer, "2 %d %d", j, i);
				rellenarCadena(buffer);
				send(receptor->socket, buffer, TAMBUF, 0);

		}
		//Si esta muerto,s recibe esto
		pareja->matriz[i][j]+= v;
		if(getppid()!=1){
			pedirSemaforo(idSemWriteShMem, 0 , -1);
			struct persona *memoria = shmat(idShMem, NULL, 0);
			memoria[0] = *emisor;
			memoria[1] = *receptor;
			devolverSemaforo(idSemReadShMem, 0 ,1);
		}

	}
	if(desconectado!=1){
	strcpy(buffer, "1 EXIT");

		send(emisor->socket, buffer, TAMBUF, 0); //para que el thread del cliente muera
		
		do{
			recv(emisor->socket, buffer, TAMBUF, 0);
			 // espera una señal del cliente para terminarr;
		}while(strcmp(buffer, "FIN"));
		if(getppid()==1){
			sprintf(buffer, "TORNEOOFF"); // 4 parametro estado
			send(emisor->socket, buffer, TAMBUF, 0);
			
		}
		else{
			sprintf(buffer, "TORNEOON"); // 4 parametro estado
			send(emisor->socket, buffer, TAMBUF, 0);
			
		}
	}
	emisor->socket=0;
	printf("SE MURIO LA COMUNICACION DE %d\n", v);
}

void rellenarCadena(char * argumento){
	int resto = 20 - strlen(argumento);
	int i;
	if(resto>1){
		strcat(argumento," ");
		for(i = 0; i < resto-2; i++)
			strcat(argumento,"-");
		strcat(argumento,"\0");
	}
}

void cerrarSocket(){
	send(socketPersonaA, "SinConexion", TAMBUF, 0);
	send(socketPersonaB, "SinConexion", TAMBUF, 0);
	close(socketPersonaA); //cada proceso cierra su socket
	close(socketPersonaB); //cada proceso cierra su socket
	perror("close");
	printf("Se desconecto el socket cliente\n");
	kill(getpid(), SIGTERM);
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
	IdSemaforo = semget(clave, cantsem,IPC_CREAT | 0660); 
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