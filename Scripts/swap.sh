#!/bin/bash

GIT_TP_LOC=~/git/tp-2015-2c-ElClan/
ADMIN_SWAP=Admin-Swap

#CONFIGURE PARAMETERS OF ADMIN_SWAP PROCESS
clear
echo "SE VA A INICIAR EL PROCESO SWAP"
echo "INGRESE EL PUERTO ESCUCHA PARA LA CONEXION CON LA MEMORIA"
read PTO_MEMORIA
echo "INGRESE EL NOMBRE DEL ARCHIVO DE SWAP"
read NOMBRE_SWAP
echo "INGRESE LA CANTIDAD DE PAGINAS"
read CANTIDAD_PAGINAS
echo "INGRESE EL TAMAÑO DE LAS PAGINAS"
read TAMANIO_PAGINA
echo "INGRESE EL RETARDO DE LA COMPACTACION"
read RETARDO_COMPACTACION


echo "PUERTO_ESCUCHA=$PTO_CPU">>  $GIT_TP_LOC/$ADMIN_SWAP/Admin-Swap.config
echo "NOMBRE_SWAP=$NOMBRE_SWAP">>  $GIT_TP_LOC/$ADMIN_SWAP/Admin-Swap.config
echo "CANTIDAD_PAGINAS=$CANTIDAD_PAGINAS">>  $GIT_TP_LOC/$ADMIN_SWAP/Admin-Swap.config
echo "TAMANIO_PAGINA=$TAMANIO_PAGINA">>  $GIT_TP_LOC/$ADMIN_SWAP/Admin-Swap.config
echo "RETARDO_COMPACTACION=$RETARDO_COMPACTACION">>  $GIT_TP_LOC/$ADMIN_SWAP/Admin-Swap.config

#Execute SWAP

echo "EXECUTING SWAP"

cd $GIT_TP_LOC/$ADMIN_SWAP

clear

./$ADMIN_SWAP Admin-Swap.conf