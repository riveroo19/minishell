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
void handler(int sig); //manejador de señales para ejecutarlas correctamente
int ourCD();
void redireccionar(int n, char *cad);//donde n puede valer 0(in), 1(out), 2(error) y cad es una cadena
void unComando(); //contemplar CD, fg, jobs y ejecucion de un único comando
void variosComandos(); //comandos con pipes vaya

//funciones lista de mandatos en bg para el jobs
//comprobar bg, si está en bg haríamos cosas con la lista y ese mandato
//createjob, createjoblist, addjob, showjobs, freejoblist, deletejob...
//

tline * line; //global para acceder desde el cd...


int main(void) {
	
	char buf[TAM];
	
    int fdin = dup(0); //0 es el fd input, 1 el de output y 2 el de error
    int fdout = dup(1);
    int fderr = dup(2);
	
	//ignoramos inicialmente las señales
	signal(SIGINT, handler); //handleamos las señales para que se ignoren y vuelvan a mostrar el prompt inicialmente
	signal(SIGQUIT, handler);

    mostrarPrompt();
	while (fgets(buf, TAM, stdin)) {
		

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
			printf("Comando a ejecutarse en background. No implementado, saliendo...\n");
			exit(1); //no tenemos bg implementado
		}


		unComando();//por dentro se analizará si es 1 comando.
		variosComandos();//por dentro se analizará si son varios comandos.

        //ponemos entrada salida y error a su fd por defecto, en caso de redirección
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
		//volvemos a ignorar las señales
		signal(SIGINT, handler);
		signal(SIGQUIT, handler);
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

void handler(int sig){
	if (sig == SIGINT){
		printf("\n");
		//mostrarPrompt();
	}
	return;
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
       			fprintf(stderr, "No existe el directorio %s o no es uno. Uso: cd <directorio>\n", dir);
       		}
    	}else{
        	fprintf(stderr,"Muchos argumentos. Uso: cd <directorio>\n"); //pone no such file or directory, luego no imprimimos el strerror
        }
		return 1; //ha ejecutado cd, si no ha funcionado ya es otra cosa...
    }
    return 0;
}



void redireccionar(int n, char *cad){ //en funcion de n, hará una u otra
	switch (n){
	case 0://entrada: bash da error si intentas hacerlo sobre un archivo que no existe, luego habrá que manejar esos posibles errores
		int aux = open(cad, O_RDONLY);
		if (aux == -1){
			fprintf(stderr, "ourshell: %s: No existe el fichero o directorio.\n", cad);
			return;
		}else{
			dup2(aux, STDIN_FILENO);
		}
		break;
	case 1://salida: bash crea un archivo si no existe, luego lo crearemos
		//dup2(open(cad, O_CREAT | O_WRONLY | O_TRUNC), n); no otorga permisos de usuario en caso de crear un nuevo archivo, luego hacemos un creat mejor (o podríamos)
		dup2(creat(cad, S_IRUSR | S_IWUSR | S_IXUSR), STDOUT_FILENO); //damos permisos en este caso sólo al usuario, se puede cambiar si no desde la shell con chmod
		break;
	case 2://error
		dup2(open(cad, O_CREAT | O_WRONLY, errno | O_TRUNC), STDERR_FILENO); //en este caso es igual que arriba pero nos interesa más así en este caso
		break;
	}
}

void unComando(){
	
    if (line->ncommands==1 && ourCD()==0){//cd se encarga de comparar si lo que se ha ejecutado es cd como tal
		//si es cd lo que se ha introducido por pantalla, habrá devuelto 0 luego entra en el if para ver si es otro comando
		
		if (strcmp(line->commands[0].argv[0], "exit")==0){//contemplamos el caso en el que se teclee exit como comando :)
			exit(0);
		}else{
			int status;
			pid_t pid = fork();
			if (pid < 0){ //ha habido un error
				fprintf(stderr, "Se ha producido un fallo en el fork...\n");
				exit(1);
			}else if (pid == 0){ //hijo
				signal(SIGINT, SIG_DFL);//ponemos esto aquí por si sucede un sleep, por ejemplo, lo haga por defecto
				signal(SIGQUIT, SIG_DFL);
				execvp(line->commands->argv[0], line->commands->argv);;
				//si es exitoso, no continuará por aquí, pero puede fallar luego:
				fprintf(stderr, "%s no es un mandato valido o su sintaxis no es correcta, prueba a hacer man %s.\n", line->commands->argv[0], line->commands->argv[0]);
				exit(1); //salida con error
			}else{//el padre
				waitpid(pid, &status, 0); //esperamos por un hijo específico
				if(WIFEXITED(status)!=0){
					if (WEXITSTATUS(status)!=0){ //si el hijo ha terminado correctamente, se ha ejecutado el comando
						fprintf(stderr, "Error en la ejecucion del mandato...\n");
					}
				}
			}

		}
	} 
}


void variosComandos(){
    //señales también para que actúen por defecto
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
    if (line->ncommands>1){
		pid_t pid;
		int status;
		int npipes = line->ncommands-1; //si hay n comandos habrá n-1 pipes

		int **pids_hijos = (int**)malloc(sizeof (int*) * npipes);
		for (int i = 0; i < npipes; i++){
			pids_hijos[i] = (int *)malloc(sizeof(int)*2);
			pipe(pids_hijos[i]); //las filas son los pipes y cada una de sus columnas sus entradas y salidas respectivas
		}

		for (int i = 0; i <= npipes; i++){
			
			pid = fork();
			if (pid<0){
				fprintf(stderr, "Se ha producido un fallo en uno de los hijos: %s\n", strerror(errno));
				exit(1);			
			}else if (pid == 0){ //if para la creación de hijos
				
				if (i==0){//creamos el primer hijo, que coge el imput de STDIN_FILENO, su descriptor de ficheros y lo pondrá en stdout del pipe, para que el siguiente hijo pueda coger su entrada de ahí proximamente
					dup2(pids_hijos[i][1], STDOUT_FILENO); 
					close(pids_hijos[i][0]);//cerramos el imput
					//primero el dup y luego ya cerramos el imput para evitar errores

					execvp(line->commands[i].argv[0], line->commands[i].argv);
					//si falla, como siempre salta aquí
					fprintf(stderr, "%s no es un mandato valido o su sintaxis no es correcta, prueba a hacer man %s.\n", line->commands[0].argv[0], line->commands[0].argv[0]);
					exit(1);
				}else if (i!=npipes && i!=0) {//creación de hijos intermedios, si hay más de 1 pipe; cogen su input de las salidas de otros pipes y lo pone en su salida para que el siguiente hijo lo pueda recibir, gracias a su pipe
					dup2(pids_hijos[i-1][0], STDIN_FILENO);
					dup2(pids_hijos[i][1], STDOUT_FILENO);
					close(pids_hijos[i-1][1]);
					close(pids_hijos[i][0]);
					
					
					execvp(line->commands[i].argv[0], line->commands[i].argv);

					fprintf(stderr, "%s no es un mandato valido o su sintaxis no es correcta, prueba a hacer man %s.\n", line->commands[i].argv[0], line->commands[i].argv[0]);
					exit(1);
				}else if (i==npipes){//último hijo que debe dejar el imput recibido del anterior hijo en STDOUT_FILENO, para que pueda mostrar por pantalla el resultado final
					dup2(pids_hijos[i-1][0], STDIN_FILENO);
					close(pids_hijos[i-1][1]);
					

					execvp(line->commands[npipes].argv[0], line->commands[npipes].argv);

					fprintf(stderr, "%s no es un mandato valido o su sintaxis no es correcta, prueba a hacer man %s.\n", line->commands[0].argv[0], line->commands[0].argv[0]);
					exit(1);
				}
			}else{//será el padre
				if (!(i==npipes)){//cerramos todos los pipes
					close(pids_hijos[i][1]);
				}
			}
		}
		for (int i = 0; i < npipes; i++){//esperamos hijo a hijo
			waitpid(pid, &status, 0);
		}
		for (int i = 0; i < npipes; i++){
			free(pids_hijos[i]);
		}
		free(pids_hijos);
		
	}
}



//Realizado por Javier Rivero Mayorga y Javier Valsera Castro
//GICIB-SSOO-2022
