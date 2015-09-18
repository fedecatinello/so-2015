/*
 ============================================================================
 Name        : Planificador.c
 Author      : Fiorillo Diego
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
/*Source file */

/* Include para las librerias */
#include <stdio.h>
#include <stdlib.h>
#include "Utils.h"
#include "Planificador.h"
#include "Consola.h"
#include "Comunicacion.h"
#include <pthread.h>
#include <unistd.h>

#define TAM_FINALIZAR -10

/** TAD para PLANIFICADOR **/
ProcesoPlanificador* arch;
/** Loggers **/
t_log* loggerInfo;
t_log* loggerError;
t_log* loggerDebug;
/** TADs **/
t_list* colaListos;
t_list* colaBlock;
t_list* colaExec;
t_list* colaFinalizados;
t_list* colaCPUs;
t_list* colaAFinalizar;
t_list* retardos_PCB;
/** ID para los PCB **/
int32_t idParaPCB = 0;
/** SOCKET SERVIDOR **/
sock_t* socketServidor;
/** Semaforos **/
sem_t semMutex_colaBlock;
sem_t semMutex_colaExec;
sem_t semMutex_colaFinalizados;
sem_t semMutex_colaListos;
sem_t semMutex_colaCPUs;
sem_t semMutex_colaAFinalizar;
sem_t sem_list_retardos;
sem_t sem_io;


ProcesoPlanificador* crear_estructura_config(char* path)
{
    t_config* archConfig = config_create(path);
    ProcesoPlanificador* config = malloc(sizeof(ProcesoPlanificador));
    config->puerto_escucha = config_get_int_value(archConfig, "PUERTO_ESCUCHA");
    config->algoritmo = config_get_string_value(archConfig, "ALGORITMO_PLANIFICACION");
    config->quantum = config_get_int_value(archConfig, "QUANTUM");
    return config;
}

/* Función que es llamada cuando ctrl+c */
void ifProcessDie(){
		log_info(loggerInfo, ANSI_COLOR_BLUE"Se dio de baja el proceso Planificador"ANSI_COLOR_RESET);
		cleanAll();
		exit(EXIT_FAILURE);
}

/*Función donde se inicializan los semaforos */

void inicializoSemaforos(){

	int32_t semMutexColaListos = sem_init(&semMutex_colaListos,0,1);
	if(semMutexColaListos==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex de Cola listos");

	int32_t semMutexColaBlock = sem_init(&semMutex_colaBlock,0,1);
	if(semMutexColaBlock==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex de Cola Block");

	int32_t semMutexColaExec = sem_init(&semMutex_colaExec,0,1);
	if(semMutexColaExec==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex de Cola Exec");

	int32_t semMutexColaFinalizados = sem_init(&semMutex_colaFinalizados,0,1);
	if(semMutexColaFinalizados==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex de Cola Finalizados");

	int32_t semMutexCpu = sem_init(&semMutex_colaCPUs,0,1);
	if(semMutexCpu==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex de Colas CPU");

	int32_t semMutexAFinalizar = sem_init(&semMutex_colaAFinalizar,0,1);
	if(semMutexAFinalizar==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex de Colas AFinalizar");

	int32_t semMutexRetardos = sem_init(&sem_list_retardos,0,1);
	if(semMutexRetardos==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex de lista retardos");

	int32_t semIO = sem_init(&sem_io, 0, 0);
	if(semIO==-1)log_error(loggerError,"No pudo crearse el semaforo Mutex para manejar la IO");

}

/*Se crea un archivo de log donde se registra to-do */

void crearArchivoDeLog() {
	char* pathLog = "Planificador.log";
	char* archLog = "Planificador";
	loggerInfo = log_create(pathLog, archLog, 0, LOG_LEVEL_INFO);
	loggerError = log_create(pathLog, archLog, 0, LOG_LEVEL_ERROR);
	loggerDebug = log_create(pathLog, archLog, 0, LOG_LEVEL_DEBUG);
}

PCB* generarPCB(int32_t PID, char* rutaArchivo){
	PCB* unPCB= malloc(sizeof(PCB));
	unPCB->PID=PID;
	unPCB->estado=LISTO;
	unPCB->ruta_archivo = malloc(string_length(rutaArchivo));
	unPCB->siguienteInstruccion = 0;
	strcpy(unPCB->ruta_archivo, rutaArchivo);
	return unPCB;

}

CPU_t* generarCPU(int32_t ID, sock_t* socketCPU){
	CPU_t* unaCPU = malloc(sizeof(CPU_t));
	unaCPU->ID=ID;
	unaCPU->socketCPU= malloc(sizeof(sock_t));
	unaCPU->socketCPU->fd = socketCPU->fd;
	unaCPU->estado = LIBRE;
	unaCPU->pcbID = 0;
	unaCPU->rendimiento=0;
	return unaCPU;
}

void creoEstructurasDeManejo(){
	colaListos=list_create();
	colaBlock=list_create();
	colaExec=list_create();
	colaFinalizados=list_create();
	colaAFinalizar=list_create();
	colaCPUs=list_create();
	retardos_PCB = list_create();

}

void cleanAll(){
	list_destroy_and_destroy_elements(colaListos, free);
	list_destroy_and_destroy_elements(colaExec, free);
	list_destroy_and_destroy_elements(colaBlock, free);
	list_destroy_and_destroy_elements(colaFinalizados, free);
	list_destroy_and_destroy_elements(retardos_PCB, free);
	log_destroy(loggerInfo);
	log_destroy(loggerError);
	log_destroy(loggerDebug);
}

/*Este metodo podria ser generico para ambos algoritmos, lo que varia seria el manejo de la "cola de listos"*/
void administrarPath(char* filePath){

	idParaPCB++;
	PCB* unPCB = generarPCB(idParaPCB, filePath);
	agregarPcbAColaListos(unPCB);
	log_debug(loggerDebug, "El PCB se genero con id %d, estado %d y mCod %s", unPCB->PID, unPCB->estado, unPCB->ruta_archivo);
	return;

}

//Funcion del hilo servidor de conexiones
void servidor_conexiones() {

	connection_pool_server_listener(socketServidor, procesarPedido);
}

void procesarPedido(sock_t* socketCPU, header_t* header){

	log_debug(loggerDebug, "Recibo operacion:%d, tamanio:%d", header->cod_op, header->size_message);
	int32_t recibido;
	int32_t cpu_id;
	int32_t tiempo;

	/** Recibo el cpu_id desde la CPU **/
	recibido = _receive_bytes(socketCPU, &(cpu_id), sizeof(int32_t));
	if(recibido == ERROR_OPERATION) return;
	log_debug(loggerDebug, "Recibo cpu_id:%d", cpu_id);


	switch(get_operation_code(header)){
	case NUEVA_CPU: {
			/** Genero un nuevo cpu_t **/
			CPU_t* nuevaCPU = generarCPU(cpu_id ,socketCPU);
			log_debug(loggerDebug, "Cpu generada correctamente");
			agregarColaCPUs(nuevaCPU);
			log_debug(loggerDebug, "Recibi una CPU nueva con id %d y socket %d", nuevaCPU->ID, nuevaCPU->socketCPU->fd);
			/** Envio el quantum a la CPU **/
			header_t* header_quantum = _create_header(ENVIO_QUANTUM, sizeof(int32_t));
			int32_t enviado = _send_header(socketCPU, header_quantum);
			if(enviado == ERROR_OPERATION)return;
			enviado = _send_bytes(socketCPU, &(arch->quantum), sizeof(int32_t));
			if(enviado == ERROR_OPERATION) return;
			log_debug(loggerDebug, "Quantum enviado exitosamente a la CPU");
			asignarPCBaCPU();
			break;
	}
	case TERMINO_RAFAGA: {
		recibirOperacion(socketCPU,cpu_id, TERMINO_RAFAGA);
		break;
	}
	case INSTRUCCION_IO: {
		recibirOperacion(socketCPU, cpu_id, INSTRUCCION_IO);
		break;
	}
	case RESULTADO_ERROR :{
		recibirOperacion(socketCPU, cpu_id, RESULTADO_ERROR);
		break;
	}
	case RESULTADO_OK: {
		recibirOperacion(socketCPU, cpu_id, RESULTADO_OK);
		break;
	}
	case UTILIZACION_CPU: {
		int32_t tiempo_uso_cpu;
		recibido = _receive_bytes(socketCPU, &tiempo_uso_cpu, sizeof(int32_t));
		if(recibido == ERROR_OPERATION){
			log_error(loggerError, "Fallo al recibir tiempo_uso_cpu (Utilizacion_CPU)");
			return;}
		log_debug(loggerDebug, "Recibi de la cpu con id: %d, el porcentaje: %d%%", cpu_id, tiempo_uso_cpu);
		/** Actualizar rendimiento de la CPU **/
		bool findCpu(void* parametro) {
			CPU_t* unaCpu = (CPU_t*) parametro;
			return unaCpu->socketCPU->fd == socketCPU->fd;
		}
		CPU_t* cpu = list_find(colaCPUs, findCpu);
		cpu->rendimiento = tiempo_uso_cpu;
		break;
	}
	default: { break;}
	}
	free(header);
}

void recibirOperacion(sock_t* socketCPU, int32_t cpu_id, int32_t cod_Operacion){
		int32_t tiempo;
		if(cod_Operacion==INSTRUCCION_IO){
			//recibe el tiempo de I/O
			int32_t recibido_tiempo = _receive_bytes(socketCPU, &(tiempo), sizeof(int32_t));
			if(recibido_tiempo == ERROR_OPERATION)return;
			log_debug(loggerDebug, "Recibo un tiempo desde la cpu, para una IO: %d", tiempo);
		}

		/** tamaño pcb **/
		int32_t tamanio_pcb;
		int32_t recibido = _receive_bytes(socketCPU, &tamanio_pcb, sizeof(int32_t));
		if(recibido == ERROR_OPERATION) return;
		log_debug(loggerDebug, "Recibo tamanio:%d", tamanio_pcb);
		char* pcb_serializado = malloc(tamanio_pcb);
		recibido = _receive_bytes(socketCPU, pcb_serializado, tamanio_pcb);
		if(recibido == ERROR_OPERATION) return;
		log_debug(loggerDebug, "Recibo un pcb desde la cpu, para una IO");
		PCB* pcb = deserializarPCB(pcb_serializado);
		/** recibo el char* de resultados **/
		int32_t tamanio_resultado_operaciones;
		recibido = _receive_bytes(socketCPU, &tamanio_resultado_operaciones, sizeof(int32_t));
		if(recibido == ERROR_OPERATION) return;
		log_debug(loggerDebug, "Recibo el resultado de operaciones de la CPU tamanio: %d", tamanio_resultado_operaciones);
		char* resultado_operaciones = malloc(tamanio_resultado_operaciones);
		recibido = _receive_bytes(socketCPU, resultado_operaciones, tamanio_resultado_operaciones);
		if(recibido == ERROR_OPERATION) return;
		log_debug(loggerDebug, "Recibo el resultado de operaciones de la CPU: %s", resultado_operaciones);
		/** Loggeo resultado operaciones Todo **/
		log_info(loggerInfo, "Recibido el proceso id: %d, ejecuto: %s",pcb->PID, resultado_operaciones);

		/** operar la respuesta **/
		if(cod_Operacion==INSTRUCCION_IO) operarIO(cpu_id, tiempo, pcb);
		if(cod_Operacion==TERMINO_RAFAGA) ;//todo
		if(cod_Operacion==RESULTADO_ERROR){
			finalizarPCB(pcb->PID, RESULTADO_ERROR);
			liberarCPU(cpu_id);
			asignarPCBaCPU();
		}
		if(cod_Operacion==RESULTADO_OK){
			finalizarPCB(pcb->PID, RESULTADO_OK);
			liberarCPU(cpu_id);
			asignarPCBaCPU();
		}

}

void liberarCPU(int32_t cpu_id){

	bool getCpuByID(CPU_t* unaCPU){
			return unaCPU->ID == cpu_id;
	}
	CPU_t* cpu_encontrada = list_find(colaCPUs, getCpuByID);
	cpu_encontrada->estado= LIBRE;
	log_debug(loggerDebug, "Cambio de estado (LIBRE) de la cpu con id %d", cpu_encontrada->ID);

}


void agregarPidAColaAFinalizar(int32_t pcbID){
	bool getPcbByID(PCB* unPCB){
			return unPCB->PID == pcbID;
		}

	if(list_any_satisfy(colaFinalizados, getPcbByID)){
			log_info(loggerInfo, "El PID indicado ya se encuentra Finalizado");
			return;
		} else{ list_add(colaAFinalizar, pcbID);
				return;}
		}


void finalizarPCB(int32_t pcbID, int32_t tipo){

	switch(tipo){

	bool getPcbByID(PCB* unPCB){
		return unPCB->PID == pcbID;
	}

	case RESULTADO_OK:{


		PCB* pcb_found = list_remove_by_condition(colaExec, getPcbByID);
		pcb_found->estado= FINALIZADO_OK;
		agregarPcbAColaFinalizados(pcb_found);
		log_debug(loggerDebug, "Cambio de estado (finalizado_ok) y de lista del pcb con id %d", pcbID);

		return;
	}

	case RESULTADO_ERROR:{

		PCB* pcb_found = list_remove_by_condition(colaExec, getPcbByID);
		pcb_found->estado= FINALIZADO_ERROR;
		agregarPcbAColaFinalizados(pcb_found);
		log_debug(loggerDebug, "Cambio de estado (finalizado_error) y de lista del pcb con id %d", pcbID);
		log_info(loggerInfo, "Se Finalizo el PCB con ID: %d", pcbID);
		return;
	}
  }
}


void operarIO(int32_t cpu_id, int32_t tiempo, PCB* pcb){

	bool getPcbByID(PCB* unPCB){
			return unPCB->PID == pcb->PID;
	}

	retardo* retardo_nuevo=malloc(sizeof(retardo));
	retardo_nuevo->ID=pcb->PID;
	retardo_nuevo->retardo=tiempo;

	/** Guardarme el PID y su retardo **/
	sem_wait(&sem_list_retardos);
	list_add(retardos_PCB, retardo_nuevo);
	sem_post(&sem_list_retardos);
	log_debug(loggerDebug, "guardo en bloqueado pid: %d, tiempo: %d", pcb->PID, tiempo);

	/** PCB de Listos a Bloqueado **/
	PCB* pcb_found = list_remove_by_condition(colaExec, getPcbByID);
	pcb_found->estado= BLOQUEADO;
	pcb_found->siguienteInstruccion=pcb->siguienteInstruccion;
	agregarPcbAColaBlock(pcb_found);
	log_debug(loggerDebug, "Cambio de estado (block) y de lista del pcb con id %d", pcb->PID);

	sem_post(&sem_io);
	/** Cambio estado CPU a LIBRE **/
	liberarCPU(cpu_id);
	/** Se asigna un nuevo PCB a la cpu q se libera **/
	asignarPCBaCPU();
}


/** Funcion del hilo de IO **/

void procesar_IO(){

	while(true) {

		sem_wait(&sem_io);

		PCB* pcb_to_sleep = list_get(colaBlock, 0);

		bool getRetardoByID(retardo* unPCB){
			return unPCB->ID == pcb_to_sleep->PID;
		}

		log_debug(loggerDebug, "Saco el pcb con id: %d", pcb_to_sleep->PID);

		/** Saco ese PID de la lista de retardos **/
		sem_wait(&sem_list_retardos);
		retardo* tiempo_retardo = list_remove_by_condition(retardos_PCB, getRetardoByID);
		sem_post(&sem_list_retardos);
		log_debug(loggerDebug, "obtengo un retardo:%d", tiempo_retardo->retardo);

		/** Simulo la IO del proceso **/
		sleep(tiempo_retardo->retardo);

		/** El proceso va a la cola de LISTOS **/
		pcb_to_sleep->estado= LISTO;
		agregarPcbAColaListos(pcb_to_sleep);

		PCB* pcb=list_remove(colaBlock, 0); //se guarda en la var inecesariamente??

		/** Se asigna un nuevo PCB a la cpu q se libera **/
		asignarPCBaCPU();

	}
}


/** Funcion que saca el tamaño de un PCB para enviar **/
int32_t obtener_tamanio_pcb(PCB* pcb) {
	return 4*sizeof(int32_t) + string_length(pcb->ruta_archivo);
}

bool hay_cpu_libre() {

	bool estaLibre(CPU_t* cpu) {
		return cpu->estado == LIBRE;
	}

	return list_any_satisfy(colaCPUs, estaLibre);
}

void cambiarAUltimaInstruccion(PCB* pcb){

	FILE* programa = fopen(pcb->ruta_archivo, "r");
	if(programa == NULL){
		log_error(loggerError, "Error al intentar abrir la ruta_archivo del pcb con id: %d", pcb->PID);
		return;
	}
	fseek(programa, TAM_FINALIZAR, SEEK_END);
	int32_t posicion = ftell(programa);
	if(posicion < 0){
		log_error(loggerError, "Error en la posicion devuelta por ftell para cambiar la instruccion");
	}
	pcb->siguienteInstruccion= posicion;
	fclose(programa);
	return;
}

void asignarPCBaCPU(){
	if(hay_cpu_libre()&& !list_is_empty(colaListos)) {
		PCB* pcbAEnviar = list_remove(colaListos, 0);

		bool getPcbByID(PCB* unPCB){
					return unPCB->PID == pcbAEnviar;
				}

		if(list_any_satisfy(colaAFinalizar, getPcbByID)){
			cambiarAUltimaInstruccion(pcbAEnviar);
			pcbAEnviar->estado = FINALIZANDO;
			sem_wait(&semMutex_colaAFinalizar);
			list_remove_by_condition(colaAFinalizar, getPcbByID);
			sem_post(&semMutex_colaAFinalizar);
			agregarPcbAColaExec(pcbAEnviar);
		}else{
			pcbAEnviar->estado = EJECUCION;
		}
		char* paquete = serializarPCB(pcbAEnviar);
		int32_t tamanio_pcb = obtener_tamanio_pcb(pcbAEnviar);
		enviarPCB(paquete, tamanio_pcb, pcbAEnviar->PID, ENVIO_PCB);
		log_info(loggerInfo, "El PCB se envio satisfactoriamente");
		/** Pongo el PCB en ejecucion **/
		agregarPcbAColaExec(pcbAEnviar);
	}

}

CPU_t* obtener_cpu_libre() {

	bool estaLibre(CPU_t* cpu) {
		return cpu->estado == LIBRE;
	}

	CPU_t* cpu_encontrada = list_find(colaCPUs, estaLibre);
	log_debug(loggerDebug, "Encontre una cpu con id %d y socket %d", cpu_encontrada->ID, cpu_encontrada->socketCPU->fd);
	return cpu_encontrada;
}

void enviarPCB(char* paquete_serializado, int32_t tamanio_pcb, int32_t pcbID, int32_t tipo){

	CPU_t* CPU = obtener_cpu_libre();
	log_debug(loggerDebug, "Cpu libre: %d socket %d y tamanio pcb %d", CPU->ID, CPU->socketCPU->fd, tamanio_pcb);
	
	if(CPU == NULL) log_debug(loggerDebug, "No encontre ninguna CPU");
	log_debug(loggerDebug, "El tamaño del serializado es %d", strlen(paquete_serializado));

	// Asigno el pcbID a la CPU_t
	CPU->pcbID = pcbID;

	int32_t enviado = send_msg(CPU->socketCPU, tipo, paquete_serializado, tamanio_pcb); //le envia el tipo de Envio
	if(enviado == ERROR_OPERATION){
		log_error(loggerError, "Fallo en el envio de PCB");
		return;
	}

	/** La cpu ahora esta en estado ocupado **/
	CPU->estado = OCUPADO;
}

void agregarPcbAColaListos(PCB* pcb){

	sem_wait(&semMutex_colaListos);
	list_add(colaListos, pcb);
	sem_post(&semMutex_colaListos);

	return;

}

void agregarPcbAColaBlock(PCB* pcb){

	sem_wait(&semMutex_colaBlock);
	list_add(colaBlock, pcb);
	sem_post(&semMutex_colaBlock);
	return;
}

void agregarPcbAColaExec(PCB* pcb){

	sem_wait(&semMutex_colaExec);
	list_add(colaExec, pcb);
	sem_post(&semMutex_colaExec);
	return;
}

void agregarPcbAColaFinalizados(PCB* pcb){


	sem_wait(&semMutex_colaExec);
	list_add(colaFinalizados, pcb);
	sem_post(&semMutex_colaExec);
	return;

}

void agregarColaCPUs(CPU_t* cpu){

	sem_wait(&semMutex_colaCPUs);
	list_add(colaCPUs, cpu);
	sem_post(&semMutex_colaCPUs);
	return;

}


void comunicarBajaACpu(int32_t pcbID){



}


//----------------------------------// MAIN //------------------------------------------//


int main(void) {

	/*Se genera el archivo de log, to-do lo que sale por pantalla */
	crearArchivoDeLog();

	/*Tratamiento del ctrl+c en el proceso */
	if(signal(SIGINT, ifProcessDie) == SIG_ERR ) log_error(loggerError,"Error con la señal SIGINT");


	/*Se genera el struct con los datos del archivo de config.- */
	char* path = "../Planificador.config";
	arch = crear_estructura_config(path);

	/*Se inicializan todos los semaforos necesarios */
	inicializoSemaforos();

	/*Se inicializan todos los elementos de gestión necesarios */
	creoEstructurasDeManejo();


	/** Creacion del socket servidor **/
	socketServidor = create_server_socket(arch->puerto_escucha);

	int32_t result = listen_connections(socketServidor);

	if(result == ERROR_OPERATION){
		log_error(loggerError, "Error al escuchar conexiones");
		exit(EXIT_FAILURE);
	}


	/** Creo hilos CONSOLA y SERVIDOR DE CONEXIONES **/
	int32_t resultado_creacion_hilos;

	pthread_t server_thread;
	resultado_creacion_hilos = pthread_create(&server_thread, NULL, servidor_conexiones, NULL);
	if (resultado_creacion_hilos != 0) {
		log_error(loggerError,"Error al crear el hilo del servidor de conexiones");
		abort();
	}else{
		log_info(loggerInfo, "Se creo exitosamente el hilo de servidor de conexiones");
	}

	pthread_t consola_thread;
	resultado_creacion_hilos = pthread_create(&consola_thread, NULL, consola_planificador, NULL);
	if (resultado_creacion_hilos != 0) {
		log_error(loggerError,"Error al crear el hilo de la consola");
		exit(EXIT_FAILURE);
	}else{
		log_info(loggerInfo, "Se creo exitosamente el hilo de la consola");
	}

	pthread_t io_thread;
	resultado_creacion_hilos = pthread_create(&io_thread, NULL, procesar_IO, NULL);
	if (resultado_creacion_hilos != 0) {
		log_error(loggerError,"Error al crear el hilo de IO");
		exit(EXIT_FAILURE);
	}else{
		log_info(loggerInfo, "Se creo exitosamente el hilo de IO");
	}

	pthread_join(server_thread, NULL);
	pthread_join(consola_thread, NULL);
	pthread_join(io_thread, NULL);

	cleanAll();

	return EXIT_SUCCESS;
}
