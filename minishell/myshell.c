#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "parser.h"

#define AZUL_PROMPT    "\x1b[34m"
#define VERDE_PROMPT	"\033[1;32m"
#define NORMAL		"\033[0m"
#define TAM 1024

void mostrarPrompt();
int ourCD();
void redireccionar(int n, char *cad);//donde n puede valer 0(in), 1(out), 2(error) y cad es una cadena(en principio no nula, para redirigir)
void unComando(char *buffer); //contemplar CD, fg, jobs y ejecucion de un único comando
void variosComandos(char *buffer); //con pipes vaya


tline * line; //global para acceder desde el cd...


int main(void) {
	
	char buf[TAM];
	
    int fdin = dup(0); //0 es el fd input, 1 el de output y 2 el de error
    int fdout = dup(1);
    int fderr = dup(2);

    //señales

    mostrarPrompt();
	while (fgets(buf, TAM, stdin)) {
		//señales?
		line = tokenize(buf);//leemos una linea de la consola
		if (line==NULL) {
			continue;
		}
		if (line->redirect_input != NULL) {
			redireccionar(0, line->redirect_input);
		}
		if (line->redirect_output != NULL) {
			redireccionar(1, line->redirect_output);
		}
		if (line->redirect_error != NULL) {
			redireccionar(2, line->redirect_error);
		}
		if (line->background) {
			printf("comando a ejecutarse en background\n");
		}


		if (line->ncommands == 1){
            unComando(buf);
        }else{
            //variosComandos(buf);
			printf("varios comandos");
        }

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
    if (line->ncommands == 1 && !strcmp(line->commands[0].argv[0], "cd")){//si hay 1 comando y es cd (!strcmp hace q compare con 0), ejecuta el cd
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
		cad = "hola";
		printf("%s", cad);
		break;
	case 1://salida

		break;
	case 2://error

		break;
	default:
		fprintf(stderr, "Error en la redirección...\n");
		break;
	}
}

void unComando(char* buffer){
    //señales?
    if (!ourCD()){//cd se encarga de comparar si lo que se ha ejecutado es cd como tal
		//si no se ha ejecutado el cd bien, habrá devuelto 0 luego entra en el if para ver si es otro comando
		getcwd(buffer, TAM);
		printf("Es otro comando %s\n", buffer);
	}

}

/*
void variosComandos(char* buffer){
    //señales??
    for (size_t i = 0; i < line->ncommands; i++){
        
    }
    
}
*/


//Realizado por Javier Rivero Mayorga y Javier Valsera Castro
//GICIB-SSOO-2022
