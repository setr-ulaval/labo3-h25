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
    /* 
    * Allows the mutex to be shared between multiple processes.
    * - Default: PTHREAD_PROCESS_PRIVATE (mutex is only accessible within a single process).
    * - PTHREAD_PROCESS_SHARED: Enables the mutex to be used across processes, 
    *   assuming it is placed in shared memory (e.g., via mmap or shm).
    */
   pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    /*
    * Enables robustness for the mutex to handle crashes.
    * - Default: PTHREAD_MUTEX_STALLED (if a thread holding the mutex crashes, it remains locked forever).
    * - PTHREAD_MUTEX_ROBUST: If a thread holding the mutex crashes, another thread can 
    *   recover the mutex by calling `pthread_mutex_consistent()` after acquiring it.
    */
   pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    /*
    * Enables priority inheritance to prevent priority inversion.
    * - Default: PTHREAD_PRIO_NONE (no priority adjustments).
    * - PTHREAD_PRIO_INHERIT: If a high-priority thread is blocked on this mutex, 
    *   the lower-priority thread holding it temporarily inherits the higher priority.
    * - PTHREAD_PRIO_PROTECT: The mutex has a fixed priority ceiling, and any thread 
    *   locking it gets that priority to prevent priority inversion.
    */
   pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
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
   zone->fd = -1;
   while (zone->fd == -1)
   {
      zone->fd = shm_open(identifiant, O_RDWR | O_CREAT, 0666); 
    //   usleep(DELAI_INIT_READER_USEC);
   }

   struct stat sb;
   memset(&sb, 0, sizeof(struct stat));
   while (sb.st_size == 0)
   {
      fstat(zone->fd, &sb);
    //   usleep(DELAI_INIT_READER_USEC);
   }

   void *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, zone->fd, 0);
   if (addr == MAP_FAILED) return -1;

   zone->header = (struct memPartageHeader *)addr;

   
   while(zone->header->frameWriter == 0)
   {
       usleep(DELAI_INIT_READER_USEC);
   }

   pthread_mutex_lock(&(zone->header->mutex));
   zone->data = (unsigned char *)((unsigned char *)addr + sizeof(struct memPartageHeader));  
   zone->tailleDonnees = zone->header->hauteur * zone->header->largeur * zone->header->canaux;
   pthread_mutex_unlock(&(zone->header->mutex));


   return 0;
}

int attenteEcrivain(struct memPartage *zone)
{
    while(zone->header->frameReader == zone->copieCompteur)
    {
        usleep(DELAI_INIT_READER_USEC);
    }
    return 0;
}

int attenteLecteur(struct memPartage *zone)
{
    while(zone->header->frameWriter == zone->copieCompteur)
    {
        usleep(DELAI_INIT_READER_USEC);
    }
    return 0;
}

int attenteLecteurAsync(struct memPartage *zone)
{

    if (zone->header->frameWriter == zone->copieCompteur) {
        // usleep(DELAI_INIT_READER_USEC);
        return -1; 
    }

    return 0;
    
}