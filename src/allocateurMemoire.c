/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de l'allocateur mémoire temps réel
 ******************************************************************************/

#include "allocateurMemoire.h"
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>


#define internal static
#define MAX_ALLOC (1280*720 *3 *sizeof(float))
#define MEM_SLOTS 10
internal char* BUFFER = NULL;

internal bool FREE_LIST[MEM_SLOTS];

void init_free_list()
{
    for (int i = 0; i < MEM_SLOTS; ++i)
    {
        FREE_LIST[i] = true;
    }
}

int prepareMemoire(size_t tailleImageEntree, size_t tailleImageSortie)
{
    (void)tailleImageEntree; // Silence l'avertissement
    (void)tailleImageSortie; // Silence l'avertissement
    assert(tailleImageEntree <= MAX_ALLOC);
    assert(tailleImageSortie <= MAX_ALLOC);
    
    if (!BUFFER)
    {
        // Allouer une zone mémoire suffisante
        BUFFER = (char*)malloc(MAX_ALLOC * MEM_SLOTS);
        if (!BUFFER) 
        {
            errno = ENOMEM;
            exit(1); 
        }

        if (mlock(BUFFER, MAX_ALLOC * MEM_SLOTS) != 0)
        {
            perror("mlock failed");
            exit(1);
        }

        init_free_list(); 
    }

    return 0;
}

// TODO: Implementez ici votre allocateur memoire utilisant l'interface decrite dans allocateurMemoire.h
void* tempsreel_malloc(size_t taille)
{
    (void)taille;
    assert (taille <= MAX_ALLOC);

    if (!BUFFER)
    {
        errno = ENOMEM;
        exit(1);
    }

    for (int i = 0; i < MEM_SLOTS; ++i)
    {
        if (FREE_LIST[i])
        {
            FREE_LIST[i] = false;
            return (void*)(BUFFER + i * MAX_ALLOC);
        }
    }

    errno = ENOMEM;
    exit(1);

    // return malloc(taille);
}   

void tempsreel_free(void* ptr)
{
    char* char_ptr = (char*)ptr;
    assert (char_ptr);
    assert (char_ptr >= BUFFER);
    assert (((char_ptr - BUFFER) % MAX_ALLOC) == 0); // PND: Make sure ptr is align on MEM_SLOTS
    
    int free_idx = (char_ptr - BUFFER) / MAX_ALLOC;
    assert (free_idx < MEM_SLOTS);
    assert(!FREE_LIST[free_idx]);

    FREE_LIST[free_idx] = true;

    // free(ptr);
}