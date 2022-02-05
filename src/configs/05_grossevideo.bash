#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
sudo ./decodeur 480p/02_Sintel.ulv /mem1 &
sleep 0.5
sudo ./redimensionneur -w 427 -h 240 -m 0 /mem1 /mem2 &
sleep 0.5
sudo ./compositeur /mem2 &
wait;

