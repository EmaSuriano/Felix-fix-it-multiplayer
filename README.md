# Felix Fix It Multiplayer

Remake of the retro game: Felix Fix It, with the ability to create rooms dinamically in order to play with another person via HTTPS. Written in C language and using SDL to render the graphics.

![Screenshot in Game](https://cloud.githubusercontent.com/assets/3399429/23585038/85bb8eea-0151-11e7-86a4-2a0641feb798.png)

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Prerequisites

Client and server will only run on Linux systems (maybe Mac too, but it hasn't been tested). In order to be able to execute we need to download some packages from apt-get install:

```
sudo apt-get install libsdl2.0-dev libsdl-image2.0-dev libsdl-ttf2.0-dev

```

### Installing

After installing all the packages from above, we're ready to compile our .c files just by running: 

```
make
```

This process will result in three new files inside the root folder:
- Torneo: server process to allow users to join to waiting rooms and play.
- Partida (CAN'T BE MANNUALLY RUN): match process - The server will create one of this when two players are ready to play.
- Cliente: client process that allow players to join rooms, change nickname and configuration.


If you want to rebuild the files, first you need to remove the compiled ones. The following command will wipe them for you!

```
makeclean
```

## Torneo(Server) description

Command to start the server:
```
./Torneo
```

This will bring a dashboard with the description of current matches and players connected: 

![Server Dashboard](https://cloud.githubusercontent.com/assets/3399429/23585040/85be054e-0151-11e7-9860-42ec3d7a3233.png)

The server will be alive until someone stop manually the process, otherwise it wouldn't close at any time.

## Cliente (Client) description

Starting the client:
```
./Cliente
```

The first screen you will see is the main Menu:
//PASTE IMAGE OF MENU

The first option will connect us to the server and join a waiting room for another player.
The second one allows us to change: 
* The configuration of the server to connect with.
* The keys to move the player and fix the windows.
* Our nickname inside the game and server.

After two players join the waiting room, the server will create a match and there they can start playing:

![Client Screens](https://cloud.githubusercontent.com/assets/3399429/23585039/85bc1c48-0151-11e7-8bab-ebf60323d3e0.png)

The match will continue until both players are dead. Meanwhile the server is storing all the points of the match, so if one person play more than one game he will gain all the points of the matches.

After the match is over, both player will enter in the waiting room (again) and the server will choose different players to play with (if it's possible). 

## Built With

* [SDL](https://www.libsdl.org/) - Simple DirectMedia Layer
* [pthread.h](http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread.h.html) - POSIX Threads
* [sem.h](http://pubs.opengroup.org/onlinepubs/7908799/xsh/syssem.h.html) - Mutex Semaphores
* [socket.h](http://pubs.opengroup.org/onlinepubs/7908799/xns/syssocket.h.html) - Sockets

## Authors

* **Emanuel Suriano** - [EmaSuriano](https://github.com/EmaSuriano)
* **Esteban Barrett** - [Ph003](https://github.com/Ph003)
* **Federico Casabona** - [FedeCasabona](https://github.com/FedeCasabona)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details
