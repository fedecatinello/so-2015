#!/bin/bash

GIT_TP_LOC=~/git/tp-2015-2c-elclan/
ADMIN_MEMORIA=Admin-Memoria

#CONFIGURE PARAMETERS OF ADMIN_MEMORIA PROCESS
clear

cd $GIT_TP_LOC/$ADMIN_MEMORIA
rm Admin-Memoria.config

echo "SE VA A INICIAR EL PROCESO ADMIN_MEMORIA"
echo "INGRESE EL PUERTO ESCUCHA PARA LA CONEXION CON CPUS"
read PTO_ESCUCHA
echo "INGRESE LA IP DEL PROCESO SWAP"
read IP_SWAP
echo "INGRESE EL PUERTO DEL PROCESO SWAP"
read PUERTO_SWAP
echo "INGRESE EL MAXIMO DE MARCOS"
read MAXIMO_MARCOS_POR_PROCESO
echo "INGRESE LA CANTIDAD DE MARCOS"
read CANTIDAD_MARCOS
echo "INGRESE EL TAMAÑO DEL MARCO"
read TAMANIO_MARCO
echo "INGRESE LA CANTIDAD DE ENTRADAS DE LA TLB"
read ENTRADAS_TLB
echo "¿TLB HABILITADA?"
read TLB_HABILITADA
echo "INGRESE EL RETARDO DEL PROCESO MEMORIA"
read RETARDO_MEMORIA
echo "INGRESE EL ALGORITMO DE REEMPLAZO DE PAGINAS"
read ALGORITMO_REEMPLAZO

echo "PUERTO_ESCUCHA=$PTO_ESCUCHA">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "IP_SWAP=$IP_SWAP">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "PUERTO_SWAP=$PUERTO_SWAP">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "MAXIMO_MARCOS_POR_PROCESO=$MAXIMO_MARCOS_POR_PROCESO">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "CANTIDAD_MARCOS=$CANTIDAD_MARCOS">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "TAMANIO_MARCO=$TAMANIO_MARCO">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "ENTRADAS_TLB=$ENTRADAS_TLB">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "TLB_HABILITADA=$TLB_HABILITADA">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "RETARDO_MEMORIA=$RETARDO_MEMORIA">>  $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config
echo "ALGORITMO_REEMPLAZO=$ALGORITMO_REEMPLAZO">> $GIT_TP_LOC/$ADMIN_MEMORIA/Admin-Memoria.config

#Execute Admin-Memoria

echo "EXECUTING ADMIN_MEMORIA"

cd $GIT_TP_LOC/$ADMIN_MEMORIA

clear

./$ADMIN_MEMORIA Admin-Memoria.config