#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
sudo ./decodeur 160p/02_Sintel.ulv /mem1 &
sudo ./decodeur 160p/02_Sintel.ulv /mem5 &
sleep 0.5
sudo ./convertisseur /mem1 /mem2 &
sudo ./convertisseur /mem5 /mem6 &
sleep 0.5
sudo ./filtreur -t 0 /mem2 /mem3 &
sudo ./filtreur -t 1 /mem6 /mem7 &
sleep 0.5
sudo ./redimensionneur -w 427 -h 240 -m 0 /mem3 /mem4 &
sudo ./redimensionneur -w 427 -h 240 -m 0 /mem7 /mem8 &
sleep 0.5
sudo ./compositeur /mem4 /mem8 &

wait;
