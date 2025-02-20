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

#define internal static
#define MAX_ALLOC (1024*1024)
#define MEM_SLOTS 5
internal char* BUFFER = NULL;

internal bool FREE_LIST[MEM_SLOTS];

void init_free_list()
{
    for (int i = 0; i < MEM_SLOTS; ++i)
    {
        FREE_LIST[i] = true;
    }
}

// TODO: Implementez ici votre allocateur memoire utilisant l'interface decrite dans allocateurMemoire.h
void* tempsreel_malloc(size_t taille)
{

    // assert (taille <= MAX_ALLOC);
    // if (!BUFFER)
    // {
    //     BUFFER = (char*)malloc(MAX_ALLOC * 5);
    //     init_free_list();
    // }

    // if (!BUFFER)
    // {
    //     errno = ENOMEM;
    //     exit(1);
    // }

    // for (int i = 0; i < MEM_SLOTS; ++i)
    // {
    //     if (FREE_LIST[i])
    //     {
    //         FREE_LIST[i] = false;
    //         return (void*)(BUFFER + i * MAX_ALLOC);
    //     }
    // }

    // errno = ENOMEM;
    // exit(1);

    return malloc(taille);
}   

void tempsreel_free(void* ptr)
{
    // char* char_ptr = (char*)ptr;
    // assert (char_ptr);
    // assert (char_ptr >= BUFFER);
    // assert (((char_ptr - BUFFER) % MAX_ALLOC) == 0); // PND: Make sure ptr is align on MEM_SLOTS
    
    // printf("Value: %d, Address: %p\n", *char_ptr, (void*)char_ptr);
    // int free_idx = (char_ptr - BUFFER) / MAX_ALLOC;
    // assert (free_idx < MEM_SLOTS);
    // assert(!FREE_LIST[free_idx]);

    // FREE_LIST[free_idx] = true;

    free(ptr);
}