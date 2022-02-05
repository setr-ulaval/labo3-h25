#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions

sudo ./decodeur 160p/03_BigBuckBunny.ulv /mem1 &
sudo ./decodeur 160p/05_Llama_drama.ulv /mem3 &
sudo ./decodeur 160p/01_ToS.ulv /mem5 &
sleep 0.5
sudo ./redimensionneur -w 427 -h 240 -m 1 /mem1 /mem2 &
sudo ./redimensionneur -w 427 -h 240 -m 1 /mem3 /mem4 &
sudo ./redimensionneur -w 427 -h 240 -m 1 /mem5 /mem6 &
sleep 0.5
sudo ./compositeur /mem2 /mem4 /mem5 /mem6 &

wait;

