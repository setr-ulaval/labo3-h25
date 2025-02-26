/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant le programme de conversion en niveaux de gris
 ******************************************************************************/

// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"


int main(int argc, char* argv[]){
    // On desactive le buffering pour les printf(), pour qu'il soit possible de les voir depuis votre ordinateur
	setbuf(stdout, NULL);
    
    // Initialise le profilage
    char signatureProfilage[128] = {0};
    char* nomProgramme = (argv[0][0] == '.') ? argv[0]+2 : argv[0];
    snprintf(signatureProfilage, 128, "profilage-%s-%u.txt", nomProgramme, (unsigned int)getpid());
    InfosProfilage profInfos;
    initProfilage(&profInfos, signatureProfilage);
    
    // Premier evenement de profilage : l'initialisation du programme
    evenementProfilage(&profInfos, ETAT_INITIALISATION);
    
    // Code lisant les options sur la ligne de commande
    char *entree, *sortie;                          // Zones memoires d'entree et de sortie
    int modeOrdonnanceur = ORDONNANCEMENT_NORT;     // NORT est la valeur par defaut
    unsigned int runtime, deadline, period;         // Dans le cas de l'ordonnanceur DEADLINE

    if(argc < 2){
        printf("Nombre d'arguments insuffisant\n");
        return -1;
    }

    if(strcmp(argv[1], "--debug") == 0){
        // Mode debug, vous pouvez changer ces valeurs pour ce qui convient dans vos tests
        printf("Mode debug selectionne pour le convertisseur niveau de gris\n");
        entree = (char*)"/mem1";
        sortie = (char*)"/mem2";
        modeOrdonnanceur = ORDONNANCEMENT_RR;
    }
    else{
        int c;
        int deadlineParamIndex = 0;
        char* splitString;

        opterr = 0;

        while ((c = getopt (argc, argv, "s:d:")) != -1){
            switch (c)
                {
                case 's':
                    // On selectionne le mode d'ordonnancement
                    if(strcmp(optarg, "NORT") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_NORT;
                    }
                    else if(strcmp(optarg, "RR") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_RR;
                    }
                    else if(strcmp(optarg, "FIFO") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_FIFO;
                    }
                    else if(strcmp(optarg, "DEADLINE") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_DEADLINE;
                    }
                    else{
                        modeOrdonnanceur = ORDONNANCEMENT_NORT;
                        printf("Mode d'ordonnancement %s non valide, defaut sur NORT\n", optarg);
                    }
                    break;
                case 'd':
                    // Dans le cas DEADLINE, on peut recevoir des parametres
                    // Si un autre mode d'ordonnacement est selectionne, ces
                    // parametres peuvent simplement etre ignores
                    splitString = strtok(optarg, ",");
                    while (splitString != NULL)
                    {
                        if(deadlineParamIndex == 0){
                            // Runtime
                            runtime = atoi(splitString);
                        }
                        else if(deadlineParamIndex == 1){
                            deadline = atoi(splitString);
                        }
                        else{
                            period = atoi(splitString);
                            break;
                        }
                        deadlineParamIndex++;
                        splitString = strtok(NULL, ",");
                    }
                    break;
                default:
                    continue;
                }
        }

        // Ce qui suit est la description des zones memoires d'entree et de sortie
        if(argc - optind < 2){
            printf("Arguments manquants (fichier_entree flux_sortie)\n");
            return -1;
        }
        entree = argv[optind];
        sortie = argv[optind+1];
    }

    printf("Initialisation convertisseur, entree=%s, sortie=%s, mode d'ordonnancement=%i\n", entree, sortie, modeOrdonnanceur);
    
    // Écrivez le code permettant de convertir une image en niveaux de gris, en utilisant la
    // fonction convertToGray de utils.c. Votre code doit lire une image depuis une zone mémoire 
    // partagée et envoyer le résultat sur une autre zone mémoire partagée.
    // N'oubliez pas de respecter la syntaxe de la ligne de commande présentée dans l'énoncé.

    setOrdonnanceur(modeOrdonnanceur, runtime, deadline, period);


    struct memPartage zone_lecteur = {0};
    
    initMemoirePartageeLecteur(entree,&zone_lecteur);

    struct memPartage zone_ecrivain = {0};
    struct memPartageHeader headerInfos = {0};
    pthread_mutex_lock(&(zone_lecteur.header->mutex));

    headerInfos.canaux = zone_lecteur.header->canaux;
    headerInfos.fps = zone_lecteur.header->fps;
    headerInfos.hauteur = zone_lecteur.header->hauteur;
    headerInfos.largeur = zone_lecteur.header->largeur;
    zone_ecrivain.tailleDonnees = headerInfos.hauteur * headerInfos.largeur;
    pthread_mutex_unlock(&(zone_lecteur.header->mutex));

    zone_ecrivain.header = &headerInfos;
    zone_ecrivain.header->canaux = 1;
    
    initMemoirePartageeEcrivain(sortie,
                            &zone_ecrivain,
                            sizeof(headerInfos)+zone_ecrivain.tailleDonnees,
                            &headerInfos);

    if(prepareMemoire(zone_lecteur.tailleDonnees, zone_ecrivain.tailleDonnees))
    {
        perror("Cannot allocate memory");
        exit(EXIT_FAILURE);
    }

    unsigned char* image_data = (unsigned char*)tempsreel_malloc(zone_lecteur.tailleDonnees);
    unsigned char* image_data_gray = (unsigned char*)tempsreel_malloc(zone_lecteur.tailleDonnees);

    pthread_mutex_lock(&(zone_ecrivain.header->mutex));
    zone_ecrivain.header->frameWriter ++;

    while(1)
    {        
        evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXLECTURE);
        pthread_mutex_lock(&(zone_lecteur.header->mutex));
        evenementProfilage(&profInfos, ETAT_TRAITEMENT);
        zone_lecteur.header->frameReader++;

        memcpy(image_data, zone_lecteur.data, zone_lecteur.tailleDonnees);
        zone_lecteur.copieCompteur = zone_lecteur.header->frameWriter;
        convertToGray(image_data, zone_lecteur.header->hauteur, zone_lecteur.header->largeur, zone_lecteur.header->canaux, image_data_gray);
        pthread_mutex_unlock(&(zone_lecteur.header->mutex));

        evenementProfilage(&profInfos, ETAT_ENPAUSE);
        attenteLecteur(&zone_lecteur);

        evenementProfilage(&profInfos, ETAT_TRAITEMENT);
        memcpy(zone_ecrivain.data, image_data_gray, zone_ecrivain.tailleDonnees);
        zone_ecrivain.copieCompteur = zone_ecrivain.header->frameReader;
        pthread_mutex_unlock(&(zone_ecrivain.header->mutex));
        
        evenementProfilage(&profInfos, ETAT_ENPAUSE);
        attenteEcrivain(&zone_ecrivain);

        evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXECRITURE);
        pthread_mutex_lock(&(zone_ecrivain.header->mutex));
        zone_ecrivain.header->frameWriter++;
    }
    
    tempsreel_free(image_data); 

    return 0;


}
