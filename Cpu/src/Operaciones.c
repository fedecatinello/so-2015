/*
 * Operaciones.c
 *
 *  Created on: 28/8/2015
 *      Author: utnso
 */

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include "Cpu.h"
#include "Operaciones.h"
extern ProcesoCPU* arch;
extern t_log* loggerInfo;
extern t_log* loggerError;
extern t_log* loggerDebug;
extern sock_t* socketPlanificador;
extern sock_t* socketMemoria;
#define TRUE 1
#define FALSE 0
int32_t quantum;

void* thread_Cpu(void* id){

	int32_t thread_id = (void*) id;

	sock_t* socket_Planificador=create_client_socket(arch->ip_planificador,arch->puerto_planificador);
	int32_t resultado = connect_to_server(socket_Planificador);
	if(resultado == ERROR_OPERATION) {
		log_error(loggerError, "Error al conectar al planificador");
		return NULL;
	}

	socketPlanificador[thread_id] = *socket_Planificador;

	//enviarle al planificador NUEVA_CPU y su id;
	header_t* header_nueva_cpu = _create_header(NUEVA_CPU, sizeof(int32_t));
	int32_t enviado =_send_header (socket_Planificador, header_nueva_cpu);

	if (enviado !=SUCCESS_OPERATION)
	{
		log_error(loggerError,"Se perdio la conexion con el Planificador");
		exit(EXIT_FAILURE);
	}

	enviado = _send_bytes(socket_Planificador, &thread_id,sizeof(int32_t));

	if (enviado !=SUCCESS_OPERATION)
	{
		log_error(loggerError,"Se perdio la conexion con el Planificador");
		exit(EXIT_FAILURE);
	}
	log_debug(loggerDebug, "Conectado con el planificador");

	free(header_nueva_cpu);


	sock_t* socket_Memoria=create_client_socket(arch->ip_memoria,arch->puerto_memoria);
	if(connect_to_server(socket_Memoria)!=SUCCESS_OPERATION){
		log_error(loggerError, "No se puedo conectar con la memoria, se aborta el proceso");
		exit(1);
	}

	log_debug(loggerDebug, "Conectado con la memoria");

	socketMemoria[thread_id] = *socket_Memoria;

	int32_t finalizar = 1;
	int32_t resul_Mensaje_Recibido;
	header_t* header_memoria = _create_empty_header() ;

	while (finalizar == 1)
	{
		resul_Mensaje_Recibido = _receive_header(socket_Planificador, header_memoria);

		if (resul_Mensaje_Recibido !=SUCCESS_OPERATION )
		{
			log_error(loggerError,"Se perdio la conexion con el Planificador");
			finalizar = 0;
			break;
		}
		tipo_Cod_Operacion (thread_id,header_memoria);

	}


	//termina el switch y termina el while, y ponele que cierro los sockets
	log_info(loggerInfo, "CPU_ID:%d->Finaliza sus tareas, hilo concluido", thread_id);

	return NULL;

}

void tipo_Cod_Operacion (int32_t id, header_t* header){

	switch (get_operation_code(header))
	{
	case ENVIO_QUANTUM:{
		int32_t recibido = _receive_bytes(&(socketPlanificador[id]), &quantum, sizeof(int32_t));
		log_info(loggerInfo,"CPU recibio de Planificador codOperacion %d QUANTUM: %d",get_operation_code(header), quantum);
		if (recibido == ERROR_OPERATION)return;
		break;
	}

	case ENVIO_PCB:{
		log_info(loggerInfo,"CPU recibio de Planificador codOperacion %d PCB",get_operation_code(header));
		char* pedido_serializado = malloc(get_message_size(header));
		int32_t recibido = _receive_bytes(&(socketPlanificador[id]), pedido_serializado, get_message_size(header));
		if(recibido == ERROR_OPERATION) {
			log_debug(loggerDebug, "Error al recibir el PCB del planificador");
			return;
		}

		log_info(loggerInfo, "Recibio el PCB correctamente");
		PCB* pcb = deserializarPCB (pedido_serializado);

		log_debug(loggerDebug, "PCB id %d estado %d ruta %s sig instruccion %d", pcb->PID, pcb->estado, pcb->ruta_archivo, pcb->siguienteInstruccion);

		ejecutar(id, pcb);

		break;
	}

	case FINALIZAR_PROCESO:{
		log_info(loggerInfo,"CPU recibio de Planificador codOperacion %d Finalizar Proceso",get_operation_code(header));
		break;
	}

	}
}




void ejecutarFIFO(int32_t id, PCB* pcb){
	log_debug(loggerDebug, "Entro a ejecutar FIFO");
	bool finalizar=false;
	char cadena[100];
	char* log_acciones=string_new();
	FILE* prog = fopen(pcb->ruta_archivo, "r");
	if (prog==NULL)
	{
		log_error (loggerError, "Error al abrir la ruta del programa");
		return;
	}
	log_debug(loggerDebug, "Siguiente Instruccion %d", pcb->siguienteInstruccion);
	fseek(prog, pcb->siguienteInstruccion, SEEK_SET);
	while(finalizar==FALSE){
		log_debug(loggerDebug, "Ejecuto una instruccion");
		if(fgets(cadena, 100, prog) != NULL)

		{

			t_respuesta* respuesta=analizadorLinea(id,pcb,cadena);
			string_append(&log_acciones, respuesta->texto);

			pcb->siguienteInstruccion=ftell(prog);

			if(respuesta->id==FINALIZAR){

				int32_t enviar_Planificador = enviarResultadoAlPlanificador(FINALIZAR,socketPlanificador);
				if(enviar_Planificador == ERROR_OPERATION) log_error(loggerError, "Error al enviar el codigo de finalizacion del proceso %d", pcb->PID);
				if(enviar_Planificador == SUCCESS_OPERATION)log_debug(loggerDebug, "Se envio el codigo de finalizacion del proceso %d", pcb->PID);

				//todo arreglar comunicacion ACA. Digito-pcb-texto?
				finalizar=TRUE;
			}

			if(respuesta->id==ENTRADASALIDA){

				int32_t enviar_Planificador = enviarResultadoAlPlanificador(ENTRADASALIDA,socketPlanificador);
				if(enviar_Planificador == ERROR_OPERATION) log_error(loggerError, "Error al enviar el codigo de Entrada/Salida del proceso %d", pcb->PID);
				if(enviar_Planificador == SUCCESS_OPERATION)log_debug(loggerDebug, "Se envio el codigo de Entrada/Salida del proceso %d", pcb->PID);

				finalizar=TRUE;
			}
			if(respuesta->id==RESULTADO_ERROR){
				int32_t enviar_Planificador = enviarResultadoAlPlanificador(RESULTADO_ERROR,socketPlanificador);
				if(enviar_Planificador == ERROR_OPERATION) log_error(loggerError, "Error al enviar el codigo de resultado error del proceso %d", pcb->PID);
				if(enviar_Planificador == SUCCESS_OPERATION)log_debug(loggerDebug, "Se envio el codigo de resultado error del proceso %d", pcb->PID);


				finalizar=TRUE;
			}
		}
		else finalizar = true;
		}


}

void ejecutar(int32_t id, PCB* pcb){
	int32_t ultimoQuantum=0;
	int32_t tamanio_pcb = obtener_tamanio_pcb(pcb);

	char cadena[100];
	char* log_acciones=string_new();
	FILE* prog = fopen(pcb->ruta_archivo, "r");
	if (prog==NULL)
	{
		log_error (loggerError, "Error al abrir la ruta del archivo");
		return;
	}
	int32_t finalizado=FALSE;
	while(finalizado == FALSE){
		if(fgets(cadena, 100, prog) != NULL)
		{
			log_debug(loggerDebug, "Tengo una linea para ejecutar");
			t_respuesta* respuesta=analizadorLinea(id,pcb,cadena);
			string_append(&log_acciones, respuesta->texto);
			pcb->siguienteInstruccion=ftell(prog);
			log_debug(loggerDebug, "Analice y ejecute una linea, la proxima tiene PC en:%d", pcb->siguienteInstruccion);
			if(respuesta->id==FINALIZAR){


				header_t* header_finalizar = _create_header(RESULTADO_OK, sizeof(int32_t));
				int32_t enviado =_send_header (socketPlanificador, header_finalizar);
				int32_t envio_id = _send_bytes(socketPlanificador,&id,sizeof(int32_t));

				char* pcb_serializado = serializarPCB(pcb);
				int32_t envio_tamanio_pcb = _send_bytes(socketPlanificador,&tamanio_pcb,sizeof(int32_t));
				int32_t envio_pcb = _send_bytes(socketPlanificador,pcb_serializado,sizeof(tamanio_pcb));

			ultimoQuantum = quantum;
			return ;
			}

			if(respuesta->id==ENTRADASALIDA){
				int32_t tamanio_texto = strlen(respuesta->texto);

				header_t* header_entrada_salida = _create_header(SOLICITUD_IO, 4*sizeof(int32_t) + tamanio_texto + tamanio_pcb);
					int32_t enviado =_send_header (socketPlanificador, header_entrada_salida);

					int32_t envio_id = _send_bytes(socketPlanificador,&id,sizeof(int32_t));

					int32_t envio_retardo = _send_bytes(socketPlanificador,&respuesta->retardo,sizeof(int32_t));

					char* pcb_serializado = serializarPCB(pcb);
					int32_t envio_tamanio_pcb = _send_bytes(socketPlanificador,&tamanio_pcb,sizeof(int32_t));
					int32_t envio_pcb = _send_bytes(socketPlanificador,pcb_serializado,sizeof(tamanio_pcb));

					int32_t envio_tamanio_texto = _send_bytes(socketPlanificador,&tamanio_texto,sizeof(int32_t));
					int32_t envio_texto = _send_bytes(socketPlanificador,respuesta->texto,sizeof(tamanio_texto));

			return ;
			}
			if(respuesta->id==ERROR){

				header_t* header_error = _create_header(RESULTADO_ERROR, 2*sizeof(int32_t)+ tamanio_pcb);
				int32_t enviado_header = _send_header(socketPlanificador, header_error);

				int32_t envio_id = _send_bytes(socketPlanificador,&id,sizeof(int32_t));

				char* pcb_serializado = serializarPCB(pcb);
				int32_t envio_tamanio_pcb = _send_bytes(socketPlanificador,&tamanio_pcb,sizeof(int32_t));
				int32_t envio_pcb = _send_bytes(socketPlanificador,pcb_serializado,sizeof(tamanio_pcb));

			return ;
			}

		} else finalizado = TRUE;

		if (quantum > 0){
					ultimoQuantum ++;
					if (ultimoQuantum > quantum)finalizado = TRUE;
			}


	}

	header_t* header_termino_rafaga = _create_header(TERMINO_RAFAGA, sizeof(int32_t)+ tamanio_pcb);
	int32_t enviado_header = _send_header(socketPlanificador, header_termino_rafaga);

	int32_t envio_id = _send_bytes(socketPlanificador,&id,sizeof(int32_t));

	char* pcb_serializado = serializarPCB(pcb);
	int32_t envio_tamanio_pcb = _send_bytes(socketPlanificador,&tamanio_pcb,sizeof(int32_t));
	int32_t envio_pcb = _send_bytes(socketPlanificador,pcb_serializado,sizeof(tamanio_pcb));

	int32_t tamanio_texto = strlen(log_acciones);
	int32_t envio_tamanio_texto = _send_bytes(socketPlanificador,&tamanio_texto,sizeof(int32_t));
	int32_t envio_texto = _send_bytes(socketPlanificador,log_acciones,sizeof(tamanio_texto));


}




t_respuesta* mAnsisOp_iniciar(int32_t id, PCB* pcb, int32_t cantDePaginas){
    //mandar al admin de memoria que se inició un proceso de N paginas

	log_debug(loggerDebug, "Se procedera a iniciar el proceso:%d", pcb->PID);
	/** Envio header a la memoria con INICIAR **/
	header_t* header_A_Memoria = _create_header(M_INICIAR, 2 * sizeof(int32_t));

	log_debug(loggerDebug, "Envie el header INICIAR con %d bytes",header_A_Memoria->size_message);

	int32_t enviado = _send_header(&(socketMemoria[id]), header_A_Memoria);
	if(enviado == ERROR_OPERATION) return NULL;

	enviado = _send_bytes(&(socketMemoria[id]),&pcb->PID, sizeof (int32_t));
	if(enviado == ERROR_OPERATION) return NULL;

	enviado = _send_bytes(&(socketMemoria[id]),&cantDePaginas, sizeof (int32_t));
	if(enviado == ERROR_OPERATION) return NULL;

	//TODO hacerlo en cada operacion
	log_debug(loggerDebug, "Envie el id %d, con %d paginas",pcb->PID, cantDePaginas);

	free(header_A_Memoria);


	/** Recibo header de la memoria con el resultado (OK o ERROR) **/
	header_t* header_de_memoria = _create_empty_header();

	int32_t recibido = _receive_header(&(socketMemoria[id]), header_de_memoria);
	t_respuesta* response= malloc(sizeof(t_respuesta));
	if(get_operation_code(header_de_memoria) == ERROR_OPERATION) {
		response->id=ERROR;
		response->texto=string_new();
		response->retardo = 0;
		string_append_with_format(&response->texto, "mProc %d - Fallo",pcb->PID);
		log_error(loggerError,"mProc %d - Fallo",pcb->PID);
	}
	else {
		response->id=INICIAR;
		response->texto=string_new();
		response->retardo = 0;
		string_append_with_format(&response->texto, "mProc %d - Iniciado",pcb->PID);
		log_info(loggerInfo,"mProc %d - Iniciado", pcb->PID);
	}

	free(header_de_memoria);
	return response;


}

int32_t enviarResultadoAlPlanificador(int32_t codigo_resultado, sock_t* socket_planificador) {

	/** Envio header al planificador con codigo de resultado (solo envia el codigo)**/
	header_t* header_planificador = _create_header(codigo_resultado, 0);

	int32_t enviado = _send_header(socket_planificador, header_planificador);

	free(header_planificador);

	if(enviado == ERROR_OPERATION) return ERROR_OPERATION;

	return SUCCESS_OPERATION;

}

t_respuesta* mAnsisOp_leer(int32_t id,PCB* pcb,int32_t numDePagina){
	//se debe leer de la memoria la pagina N
	log_debug(loggerDebug, "Se procedera a leer del proceso:%d, la pagina:%d", pcb->PID, numDePagina);

	/** Envio header a la memoria con LEER **/
	header_t* header_A_Memoria = _create_header(M_LEER, 3 * sizeof(int32_t));

	int32_t enviado = _send_header(&(socketMemoria[id]), header_A_Memoria);
	if(enviado == ERROR_OPERATION) return NULL;

	enviado = _send_bytes(&(socketMemoria[id]),&pcb->PID, sizeof (int32_t));
	if(enviado == ERROR_OPERATION) return NULL;

	enviado = _send_bytes(&(socketMemoria[id]),&numDePagina, sizeof (int32_t));
	if(enviado == ERROR_OPERATION) return NULL;

	int32_t tamanio=0;
	enviado = _send_bytes(&(socketMemoria[id]),&tamanio, sizeof (int32_t));
	if(enviado == ERROR_OPERATION) return NULL;

	log_debug(loggerDebug, "Envie el id %d,el numero de pagina %d",pcb->PID, numDePagina);

	/** Recibo header de la memoria con el codigo CONTENIDO_PAGINA o NULL **/
	header_t* header_de_memoria = _create_empty_header();

	int32_t recibido = _receive_header(&(socketMemoria[id]), header_de_memoria);

	int32_t longPagina;

	int32_t recibi_longPagina = _receive_bytes(&(socketMemoria[id]), &longPagina, sizeof(int32_t));
	if(recibi_longPagina == ERROR_OPERATION) return NULL;

	char* contenido_pagina = malloc(longPagina);
	recibido = _receive_bytes(&(socketMemoria[id]), contenido_pagina, longPagina);
	contenido_pagina[longPagina]='\0';

	if(recibido == ERROR_OPERATION) return NULL;
	t_respuesta* response= malloc(sizeof(t_respuesta));
	if (contenido_pagina != NULL){
	response->id=LEER;
	response->texto=string_new();
	response->retardo = 0;

	string_append_with_format(&response->texto, "mProc %d - Pagina %d leida: %s ",pcb->PID, numDePagina, contenido_pagina);
	log_info(loggerInfo,"mProc %d - Pagina %d leida: %s ",pcb->PID, numDePagina, contenido_pagina);
	}
	else{
		response->id=ERROR;
		response->texto=string_new();
		response->retardo = 0;

		string_append_with_format(&response->texto, "Error en el mProc %d - Pagina %d no leida: %s ",pcb->PID, numDePagina, contenido_pagina);
		log_error(loggerError,"Error en el mProc %d - Pagina %d NO leida: %s ",pcb->PID, numDePagina, contenido_pagina);

	}

	return response;
}

t_respuesta* mAnsisOp_escribir(int32_t id,PCB* pcb, int32_t numDePagina, char* texto){
	//se debe escribir en memoria el texto en la pagina N
	log_debug(loggerDebug, "Se procedera a esribir del proceso:%d, la pagina: %d, %d bytes",pcb->PID, numDePagina, strlen(texto));
	/** Envio header a la memoria con ESCRIBIR **/
	int32_t tamanio = strlen(texto);
	header_t* header_A_Memoria = _create_header(M_ESCRIBIR, 3 * sizeof(int32_t)+tamanio);

	int32_t enviado_Header = _send_header(&(socketMemoria[id]), header_A_Memoria);

	int32_t enviado_Id_Proceso = _send_bytes(&(socketMemoria[id]),&pcb->PID, sizeof (int32_t));
	int32_t enviado_numDePagina = _send_bytes(&(socketMemoria[id]),&numDePagina, sizeof (int32_t));
	int32_t enviado_tamanio_texto = _send_bytes(&(socketMemoria[id]),&tamanio, sizeof (int32_t));
	int32_t enviado_texto = _send_bytes(&(socketMemoria[id]),texto, tamanio);

	log_debug(loggerDebug, "Envie el id %d,el numero de pagina %d,y el texto %s",pcb->PID, numDePagina, texto);
	/** Recibo header de la memoria con el resultado (OK o ERROR) **/
	header_t* header_de_memoria = _create_empty_header();

	int32_t recibido = _receive_header(&(socketMemoria[id]), header_de_memoria);
	//todo sacar el error o el ok y de ahi cambiar a error o nada
	t_respuesta* response= malloc(sizeof(t_respuesta));

	if (header_de_memoria->cod_op == OK){
		response->id=ESCRIBIR;
		response->texto=string_new();
		response->retardo = 0;

		string_append_with_format(&response->texto, "mProc %d - Pagina %d escrita: %s", pcb->PID,numDePagina, texto);

		log_info(loggerInfo, "mProc %d - Pagina %d escrita: %s", pcb->PID,numDePagina, texto);
	} else {

		response->id=ERROR;
		response->texto=string_new();
		response->retardo = 0;

		string_append_with_format(&response->texto, "Error en el mProc %d - Pagina %d NO escrita: %s", pcb->PID,numDePagina, texto);

		log_error(loggerError, "Error en el mProc %d - Pagina %d NO escrita: %s", pcb->PID,numDePagina, texto);

	}


	return response;

}

t_respuesta* mAnsisOp_IO(int32_t id, PCB* pcb,int32_t tiempo){
	//decirle al planificador que se haga una i/o de cierto tiempo
	//solo ver si se envio al planificador
	log_debug(loggerDebug, "Se procedera a enviar el proceso:%d a IO con retardo: %d", pcb->PID, tiempo);

	//TODO como se libera la CPU?
	header_t* header_A_Planificador = _create_header(SOLICITUD_IO,2*sizeof(int32_t));

//	int32_t enviado_Header = _send_header(&(socketPlanificador[id]),header_A_Planificador);
//
//	int32_t enviado_Id_Proceso = _send_bytes(&(socketPlanificador[id]),&pcb->PID,sizeof(int32_t));
//	int32_t enviado_Tiempo = _send_bytes(&(socketPlanificador[id]),&tiempo,sizeof(int32_t));
//
//	int32_t resultado;
//	int32_t recibi_Resultado = _receive_bytes(&(socketPlanificador[id]),&resultado,sizeof (int32_t));
//
//	if(recibi_Resultado == ERROR_OPERATION) return NULL;


	//todo, pasarle el tiempo de IO
	t_respuesta* response= malloc(sizeof(t_respuesta));

	response->id=ENTRADASALIDA;
	response->texto=string_new();
	response->retardo = tiempo;
	string_append_with_format(&response->texto, "mProc %d en entrada-salida de tiempo %d", id,tiempo);
	log_info(loggerInfo, "mProc %d en entrada-salida de tiempo %d", id,tiempo);

	return response;
}

t_respuesta* mAnsisOp_finalizar(int32_t id, PCB* pcb){
		//decirle a la memoria que se elimine la tabla asociada
	log_debug(loggerDebug, "Se procedera a finalizar el proceso:%d", pcb->PID);

	header_t* header_A_Memoria = _create_header(M_FINALIZAR, sizeof(int32_t));

	int32_t enviado_Header = _send_header(&(socketMemoria[id]),header_A_Memoria);
	int32_t enviado_Id_Proceso = _send_bytes(&(socketMemoria[id]),&pcb->PID, sizeof (int32_t));

	log_debug(loggerDebug, "Envie el id %d de finalizar",pcb->PID);

	int32_t resultado;
	int32_t resultado_Mensaje = _receive_bytes(&(socketMemoria[id]),&resultado, sizeof (int32_t)); //aca recibo un que?
	if(resultado_Mensaje == ERROR_OPERATION) return NULL;

	t_respuesta* response= malloc(sizeof(t_respuesta));

	if (resultado_Mensaje ==OK){

	response->id=FINALIZAR;
	response->texto=string_new();
	response->retardo = 0;
	string_append_with_format(&response->texto, "mProc %d Finalizado", pcb->PID);
	log_info(loggerInfo, "mProc %d Finalizado", pcb->PID);

	}else
	{
		response->id=ERROR;
		response->texto=string_new();
		response->retardo = 0;
		string_append_with_format(&response->texto, "Error al finalizar el mProc %d ", pcb->PID);
		log_error(loggerError, "Error al finalizar el mProc %d ", pcb->PID);
	}

	return response;

}

//Analizador de linea
t_respuesta* analizadorLinea(int32_t id,PCB* pcb, char* const instruccion){

	char* linea= strdup(instruccion);
	switch (analizar_operacion_asociada(linea)){
	case INICIAR:{
		int cantDePaginas=buscarPrimerParametro(strstr(instruccion," ")+1);
		free(linea);
		return mAnsisOp_iniciar(id,pcb, cantDePaginas);
		break;
		}
	case FINALIZAR:{
		free(linea);
		return mAnsisOp_finalizar(id, pcb);
		break;
		}
	case ENTRADASALIDA:{
		int tiempo=buscarPrimerParametro(strstr(instruccion," ")+1);
		free(linea);
		return mAnsisOp_IO(id,pcb,tiempo);
		break;
		}
	case LEER:{
		int numDePagina=buscarPrimerParametro(strstr(instruccion," ")+1);
		free(linea);
		return mAnsisOp_leer(id,pcb, numDePagina);
		break;
		}
	case ESCRIBIR:{
		structEscribir parametros=buscarAmbosParametros(strstr(instruccion," ")+1);
		free(linea);
		return mAnsisOp_escribir(id,pcb,parametros.pagina, parametros.texto);
		break;
		}
	default:
		printf("Linea no válida\n");
		free(linea);
		return NULL;
		break;
	}

	return NULL;
}
