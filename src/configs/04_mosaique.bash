#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
sudo ./decodeur 240p/02_Sintel.ulv /mem1 &
sudo ./decodeur 240p/01_ToS.ulv /mem2 &
sudo ./decodeur 240p/03_BigBuckBunny.ulv /mem3 &
sudo ./decodeur 240p/04_Caminandes.ulv /mem4 &
sleep 0.5
sudo ./compositeur /mem1 /mem2 /mem3 /mem4 &
wait;

