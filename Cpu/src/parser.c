#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "commons/string.h"
#include "parser.h"

/*FUNCIONES PRIVADAS*/
int32_t analizar_operacion_asociada(char* operacion) {

	if(string_starts_with(operacion, "iniciar")) return INICIAR;
	if(string_starts_with(operacion, "finalizar")) return FINALIZAR;
	if(string_starts_with(operacion, "entrada-salida")) return ENTRADASALIDA;
	if(string_starts_with(operacion, "leer")) return LEER;
	if(string_starts_with(operacion, "escribir")) return ESCRIBIR;
	return ERROR;
}
int esEspacio(char caracter){return(caracter==' ')?1:0;}

int esPuntoyComa(char caracter){return(caracter==';')?1:0;}

int buscarPrimerParametro(char*linea){
	char* parametro=string_new();
	while(!esPuntoyComa(*linea)){//este while sirve si llees la instruccion con el ; final
		string_append_with_format(&parametro,"%c",*linea);
		linea++;
	}
	return atoi(parametro);
}

char* buscarSegundoParametro(char*linea){
	char* parametro2=string_new();
	while(!esPuntoyComa(*linea)){//este while sirve si lees la instruccion con el ; final
		string_append_with_format(&parametro2,"%c",*linea);
		linea++;
	}
	return parametro2;
}

structEscribir buscarAmbosParametros(char*linea){
	char*parametro1=string_new();
	while(!esEspacio(*linea)){//este while sirve si lees la instruccion con el ; final
		string_append_with_format(&parametro1,"%c",*linea);
		linea++;
	}
	linea++;
	char*parametro2=buscarSegundoParametro(linea);
	structEscribir parametros;
	parametros.pagina=atoi(parametro1);
	parametros.tamanio_texto=strlen(parametro2);
	parametros.texto=parametro2;
	return parametros;

}

//funciones mAnsisOp
void mAnsisOp_iniciar(int cantDePaginas){
	printf("iniciar %d;\n",cantDePaginas);
}

void mAnsisOp_leer(int numDePagina){
	printf("leer %d;\n",numDePagina);
}

void mAnsisOp_escribir(int numDePagina, char* texto){
	printf("escribir %d %s;\n",numDePagina,texto);
}

void mAnsisOp_IO(int tiempo){
	printf("IO %d;\n",tiempo);
}

void mAnsisOp_finalizar(){
	printf("finalizar;\n");
}

//Analizador de linea
void analizadorLinea(char* const instruccion){

	char* linea= strdup(instruccion);
	switch (analizar_operacion_asociada(linea)){
	case INICIAR:{
		int cantDePaginas=buscarPrimerParametro(strstr(instruccion," ")+1);
		mAnsisOp_iniciar(cantDePaginas);
		break;
		}
	case FINALIZAR:{
		mAnsisOp_finalizar();
		break;
		}
	case ENTRADASALIDA:{
		int tiempo=buscarPrimerParametro(strstr(instruccion," ")+1);
		mAnsisOp_IO(tiempo);
		break;
		}
	case LEER:{
		int numDePagina=buscarPrimerParametro(strstr(instruccion," ")+1);
		mAnsisOp_leer(numDePagina);
		break;
		}
	case ESCRIBIR:{
		structEscribir parametros=buscarAmbosParametros(strstr(instruccion," ")+1);
		mAnsisOp_escribir(parametros.pagina, parametros.texto);
		break;
		}
	default:
		printf("Linea no válida\n");
		break;
	}
	free(linea);
}



