#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "parser.h"

#define AZUL_PROMPT    "\x1b[34m"
#define VERDE_PROMPT	"\033[1;32m"
#define NORMAL		"\033[0m"
#define TAM 1024

void mostrarPrompt();
int ourCD();
void redireccionar(int n, char *cad);//donde n puede valer 0(in), 1(out), 2(error) y cad es una cadena(en principio no nula, para redirigir)
void unComando(); //contemplar CD, fg, jobs y ejecucion de un único comando
void variosComandos(char *buffer); //con pipes vaya


tline * line; //global para acceder desde el cd...


int main(void) {
	
	char buf[TAM];
	
    int fdin = dup(0); //0 es el fd input, 1 el de output y 2 el de error
    int fdout = dup(1);
    int fderr = dup(2);
	//ignoramos inicialmente las señales
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

    mostrarPrompt();
	while (fgets(buf, TAM, stdin)) {
		//de momento ignoramos las señales (si haces ctrl+c en bash no sale de la consola, luego nos interesa ignorarlas de momento)
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);

		line = tokenize(buf);//leemos una linea de la consola
		
		if (line==NULL) {
			continue;
		}
		if (line->redirect_input != NULL) {
			//redireccionar(0, line->redirect_input);
		}
		if (line->redirect_output != NULL) {
			//redireccionar(1, line->redirect_output);
		}
		if (line->redirect_error != NULL) {
			//redireccionar(2, line->redirect_error);
		}
		if (line->background) {
			printf("Comando a ejecutarse en background. No implementado, saliendo...\n");
			exit(1); //no tenemos bg implementado
		}


		unComando(buf);//por dentro se analizará si es 1 comando.
		//variosComandos(buf);//por dentro se analizará si son varios comandos.

        //ponemos entrada salida y error por defecto
        if(line->redirect_input != NULL){
            dup2(fdin, 0);
        }
        if (line->redirect_output != NULL){
            dup2(fdout, 1);
        }
        if (line->redirect_error != NULL){
            dup2(fderr, 2);
        }
        mostrarPrompt();
	}
	return 0;
}

void mostrarPrompt(){
    char aux[TAM];
    getcwd(aux, TAM);
    printf(VERDE_PROMPT"ourshell");
    printf(NORMAL":");
    printf(AZUL_PROMPT"~%s", aux);
    printf(NORMAL"$ ");
}

int ourCD(){
    if (!strcmp(line->commands[0].argv[0], "cd")){//si hay 1 comando() y es cd (!strcmp hace q compare con 0), ejecuta el cd
    	//printf("se detecta");
    	if (line->commands[0].argc <= 2){//se puede ejecutar el CD.
    		char *dir;
       		if (line->commands[0].argc == 1){
       			dir = getenv("HOME");
       			if (dir==NULL){
       				fprintf(stderr, "No hay variable $HOME en el sistema. Falló el cd...\n");
					//hacemos el return después al comprobar que no es un directorio, ya que no existe la variable home y dir valdrá null
       			}
       		}else{
       			dir = line->commands[0].argv[1]; 
       		}
       		if (chdir(dir)!=0){
       			fprintf(stderr, "No existe el directorio %s o no es uno. Uso: cd <directorio>\n%s\n", dir, strerror(errno));
				return 0;
       		}else{
				return 1;
			}
    	}else{
        	fprintf(stderr,"Muchos argumentos. Uso: cd <directorio>\n"); //pone no such file or directory, luego no imprimimos el strerror
			return 0;
        }
    }
    return 0;
}



void redireccionar(int n, char *cad){ //dependiendo de lo que le pasemos, hará red de entrada, salida o error
	switch (n){
	case 0://entrada
		dup2(open(cad, O_RDONLY | O_CREAT), n); //como si no existe el fichero de entrada lo crea, no va a dar error
		break;
	case 1://salida
		dup2(open(cad, O_CREAT | O_WRONLY | O_TRUNC), n); //si no existe el fichero donde escribir, lo crea y si no escribe o sobreescribe
		break;
	case 2://error
		dup2(open(cad, O_CREAT | O_WRONLY, errno | O_TRUNC), n);//escribirá errno en dicho fichero, si no existe lo crea
		break;
	}
}

void unComando(){
	//ahora sí, activamos las señales para que actuen por defecto
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
    if (line->ncommands==1 && !ourCD()){//cd se encarga de comparar si lo que se ha ejecutado es cd como tal
		//si no se ha ejecutado el cd bien, habrá devuelto 0 luego entra en el if para ver si es otro comando
		
		if (strcmp(line->commands[0].argv[0], "exit")==0){//contemplamos el caso en el que se teclee exit como comando :)
			exit(0);
		}else{
			int status;
			pid_t pid = fork();
			if (pid < 0){ //ha habido un error
				line->redirect_error = "Se ha producido un fallo en el fork...";
				fprintf(stderr, "Se ha producido un fallo en el fork...\n");
				exit(1);
			}else if (pid == 0){ //hijo
				execvp(line->commands->argv[0], line->commands->argv);;
				//si es exitoso, no continuará por aquí, pero puede fallar luego:
				line->redirect_error = "Ha habido un error con la ejecucion del mandato...";
				fprintf(stderr, "%s no es un mandato valido o su sintaxis no es correcta, prueba a hacer man %s.\n", line->commands->argv[0], line->commands->argv[0]);
				exit(1); //salida con error
			}else{//el padre
				waitpid(pid, &status, 0); //esperamos por un hijo específico
				if(WIFEXITED(status)!=0 && WEXITSTATUS(status)!=0){ //si el hijo ha terminado correctamente, se ha ejecutado el comando
					fprintf(stderr, "Error en la ejecucion del mandato...\n");
				}
			}

		}
	}
}

/*
void variosComandos(char* buffer){
    //señales también para que actúen por defecto
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
    for (size_t i = 0; i < line->ncommands; i++){
        
    }
    
}
*/


//Realizado por Javier Rivero Mayorga y Javier Valsera Castro
//GICIB-SSOO-2022
