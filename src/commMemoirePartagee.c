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
   shm_unlink(identifiant);
   zone->fd = shm_open(identifiant, O_RDWR | O_CREAT, 0777); 
   if (zone->fd == -1) {
        printf("shm_open failed: %s\n", strerror(errno));
        return -1;
    }

   if (ftruncate(zone->fd, taille) == -1) {
      close(zone->fd);
      shm_unlink(identifiant);
      return -1;
   }

   void * addr = mmap(NULL, taille, PROT_READ | PROT_WRITE, MAP_SHARED, zone->fd, 0);
   if (addr == MAP_FAILED) {
      close(zone->fd);
      shm_unlink(identifiant);
      return -1;
   }

   memset(addr, 0, taille); 
   memcpy(addr, headerInfos, sizeof(struct memPartageHeader));
   zone->header = (struct memPartageHeader *)addr;
   zone->data = (unsigned char *)((unsigned char *)addr + sizeof(struct memPartageHeader));   

   // Configuration du mutex pour le partage entre processus
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
   int res = pthread_mutex_init(&(zone->header->mutex), &attr);
   pthread_mutexattr_destroy(&attr);
   if (res != 0) {
      munmap(addr, taille);
      close(zone->fd);
      shm_unlink(identifiant);
      return -1;
   }

   return 0;
}

int initMemoirePartageeLecteur(const char* identifiant,
                                struct memPartage *zone)
{
   struct stat sb;
   memset(&sb, 0, sizeof(struct stat));
   while (zone->fd == -1 || sb.st_size == 0)
   {
      zone->fd = shm_open(identifiant, O_RDWR | O_CREAT, 0666); 
      fstat(zone->fd, &sb);
   }

   void *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, zone->fd, 0);
   if (addr == MAP_FAILED) return -1;

   zone->header = (struct memPartageHeader *)addr;
   zone->data = (unsigned char *)((unsigned char *)addr + sizeof(struct memPartageHeader));  

   pthread_mutex_lock(&(zone->header->mutex));
   zone->tailleDonnees = zone->header->hauteur * zone->header->largeur * zone->header->canaux;
   pthread_mutex_unlock(&(zone->header->mutex));

   return 0;
}

int attenteEcrivain(struct memPartage *zone)
{
    pthread_mutex_lock(&(zone->header->mutex));
    while(zone->header->frameReader == zone->copieCompteur)
    {
        pthread_mutex_unlock(&(zone->header->mutex));
        usleep(DELAI_INIT_READER_USEC);
        pthread_mutex_lock(&(zone->header->mutex));
    }
    pthread_mutex_unlock(&(zone->header->mutex));
    return 0;
}

int attenteLecteur(struct memPartage *zone)
{
    pthread_mutex_lock(&(zone->header->mutex));
    while(zone->header->frameWriter == zone->copieCompteur)
    {
        pthread_mutex_unlock(&(zone->header->mutex));
        usleep(DELAI_INIT_READER_USEC);
        pthread_mutex_lock(&(zone->header->mutex));
    }
    pthread_mutex_unlock(&(zone->header->mutex));
    return 0;
}

int attenteLecteurAsync(struct memPartage *zone)
{
    int result = 0;
    pthread_mutex_lock(&(zone->header->mutex));
    
    if (zone->header->frameWriter == zone->copieCompteur) {
        result = -1; // Toujours pas de nouveau frame
    }
    
    pthread_mutex_unlock(&(zone->header->mutex));
    return result;
}