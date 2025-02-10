/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de communication inter-processus
 ******************************************************************************/

#include "commMemoirePartagee.h"

// TODO: implementez ici les fonctions decrites dans commMemoirePartagee.h

int initMemoirePartageeEcrivain(const char* identifiant,
                                struct memPartage *zone,
                                size_t taille,
                                struct memPartageHeader* headerInfos)
{
   zone->fd = shm_open(identifiant, O_RDWR | O_CREAT, 0666); 
   if (zone->fd == -1) return -1;
   if (ftruncate(zone->fd, taille) == -1) return -1;
   void * addr = mmap(NULL, taille, PROT_READ | PROT_WRITE, MAP_SHARED, zone->fd, 0);
   if (addr == MAP_FAILED) return -1;
   memset(addr, 0, taille); 
   memcpy(addr, headerInfos, sizeof(struct memPartageHeader));
   zone->header = (struct memPartageHeader *)addr;
   zone->data = (unsigned char *)(addr + sizeof(struct memPartageHeader));   
   pthread_mutex_init(&(zone->header->mutex), NULL);  
   pthread_mutex_lock(&(zone->header->mutex));
   zone->header->frameWriter = 1;

   return 0;
}


int attenteEcrivain(struct memPartage *zone)
{
    pthread_mutex_lock(&(zone->header->mutex));
    int frameReader = zone->header->frameReader;
    pthread_mutex_unlock(&(zone->header->mutex));
    while(frameReader == zone->copieCompteur)
    {
        pthread_mutex_lock(&(zone->header->mutex));
        frameReader = zone->header->frameReader;
        pthread_mutex_unlock(&(zone->header->mutex));
    }
    return 0;
}
