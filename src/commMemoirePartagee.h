/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier de déclaration des fonctions de communication entre les programmes
 * Ne modifiez pas les structs et prototypes de fonction écrits ici.
 ******************************************************************************/

#ifndef COMM_MEM_H
#define COMM_MEM_H

#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#define DELAI_INIT_READER_USEC 1000

/* Architecture du buffer partagé (voir l'énoncé pour plus de détails) :
 *
 * Offset     Taille     Type      Description
 * 0           24        pthread_mutex_t     Mutex
 * 24          4         int32     Index trame writer
 * 28          4         int32     Index trame reader
 * 32          2         uint16    Hauteur (en pixels)
 * 34          2         uint16    Largeur (en pixels)
 * 36          2         uint16    Nombre de canaux (1 ou 3)
 * 38          2         uint16    Nombre d'images par seconde
 * 40          w*h*c     uint8     Données
 */

// Le reste de ce fichier constitue une suggestion de structures et fonctions
// à créer pour lire et écrire l'espace mémoire partagé.


// Cette structure permet d'accéder facilement aux diverses informations stockées
// au début de l'espace partagé
struct memPartageHeader{
    pthread_mutex_t mutex;
    uint32_t frameWriter;
    uint32_t frameReader;
    uint16_t hauteur;
    uint16_t largeur;
    uint16_t canaux;
    uint16_t fps;
};

// Cette structure permet de mémoriser l'information sur une zone mémoire partagée.
// C'est un pointeur vers une instance de cette structure qui sera passée aux
// différentes fonctions
struct memPartage{
    int fd;
    struct memPartageHeader *header;
    size_t tailleDonnees;
    unsigned char* data;
    uint32_t copieCompteur;             // Permet de se rappeler le compteur de l'autre processus
};

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas du lecteur)
int initMemoirePartageeLecteur(const char* identifiant,
                                struct memPartage *zone);

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas de l'écrivain)
int initMemoirePartageeEcrivain(const char* identifiant,
                                struct memPartage *zone,
                                size_t taille,
                                struct memPartageHeader* headerInfos);

// Appelé par le lecteur pour se mettre en attente d'un résultat
int attenteLecteur(struct memPartage *zone);

// Fonction spéciale similaire à attenteLecteur, mais asynchrone : cette fonction ne bloque jamais.
// Cela est utile pour le compositeur, qui ne doit pas bloquer l'entièreté des flux si un seul est plus lent.
int attenteLecteurAsync(struct memPartage *zone);

// Appelé par l'écrivain pour se mettre en attente de la lecture du résultat précédent par un lecteur
int attenteEcrivain(struct memPartage *zone);

// N'oubliez pas de créer le fichier commMemoirePartagee.c et d'y implémenter les fonctions décrites ici!

#endif
