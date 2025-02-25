/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant le programme de redimensionnement d'images
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
    
    
    // Écrivez le code permettant de redimensionner une image (en utilisant les fonctions précodées
    // dans utils.c, celles commençant par "resize"). Votre code doit lire une image depuis une zone 
    // mémoire partagée et envoyer le résultat sur une autre zone mémoire partagée.
    // N'oubliez pas de respecter la syntaxe de la ligne de commande présentée dans l'énoncé.

        // Code lisant les options sur la ligne de commande
    char *entree, *sortie;                          // Zones memoires d'entree et de sortie
    int modeOrdonnanceur = ORDONNANCEMENT_NORT;     // NORT est la valeur par defaut
    unsigned int runtime, deadline, period;         // Dans le cas de l'ordonnanceur DEADLINE

    if(argc < 2){
        printf("Nombre d'arguments insuffisant\n");
        return -1;
    }
    int width = 0, height = 0, resizeMethod = -1; // Ajout des nouvelles options
    if(strcmp(argv[1], "--debug") == 0){
        // Mode debug, vous pouvez changer ces valeurs pour ce qui convient dans vos tests
        printf("Mode debug selectionne pour le convertisseur niveau de gris\n");
        entree = (char*)"/mem1";
        sortie = (char*)"/mem2";
        width = 427;
        height = 240;
        resizeMethod = 0;

    }
    else
    {
        int c;
        int deadlineParamIndex = 0;
        char* splitString;

        opterr = 0;

        while ((c = getopt(argc, argv, "s:d:w:h:r:")) != -1) {
            switch (c) {
                case 's':
                    // On sélectionne le mode d'ordonnancement
                    if (strcmp(optarg, "NORT") == 0) {
                        modeOrdonnanceur = ORDONNANCEMENT_NORT;
                    } else if (strcmp(optarg, "RR") == 0) {
                        modeOrdonnanceur = ORDONNANCEMENT_RR;
                    } else if (strcmp(optarg, "FIFO") == 0) {
                        modeOrdonnanceur = ORDONNANCEMENT_FIFO;
                    } else if (strcmp(optarg, "DEADLINE") == 0) {
                        modeOrdonnanceur = ORDONNANCEMENT_DEADLINE;
                    } else {
                        modeOrdonnanceur = ORDONNANCEMENT_NORT;
                        printf("Mode d'ordonnancement %s non valide, défaut sur NORT\n", optarg);
                    }
                    break;

                case 'd':
                    // Gestion des paramètres pour le mode DEADLINE
                    splitString = strtok(optarg, ",");
                    while (splitString != NULL) {
                        if (deadlineParamIndex == 0) {
                            runtime = atoi(splitString);
                        } else if (deadlineParamIndex == 1) {
                            deadline = atoi(splitString);
                        } else {
                            period = atoi(splitString);
                            break;
                        }
                        deadlineParamIndex++;
                        splitString = strtok(NULL, ",");
                    }
                    break;

                case 'w':
                    width = atoi(optarg);
                    if (width <= 0) {
                        printf("Largeur invalide: %s\n", optarg);
                        width = 0;
                    }
                    break;

                case 'h':
                    height = atoi(optarg);
                    if (height <= 0) {
                        printf("Hauteur invalide: %s\n", optarg);
                        height = 0;
                    }
                    break;

                case 'r':
                    resizeMethod = atoi(optarg);
                    if (resizeMethod != 0 && resizeMethod != 1) {
                        printf("Méthode de redimensionnement invalide: %s. Utiliser 0 pour plus proche voisin ou 1 pour interpolation bilinéaire.\n", optarg);
                        resizeMethod = -1;
                    }
                    break;

                default:
                    continue;
            }
        }

        if(argc - optind < 2){
            printf("Arguments manquants (fichier_entree flux_sortie)\n");
            return -1;
        }
        entree = argv[optind];
        sortie = argv[optind+1];
    }

    setOrdonnanceur(modeOrdonnanceur, runtime, deadline, period);

    // Vérification des paramètres du redimensionnement
    if (width > 0 && height > 0 && (resizeMethod == 0 || resizeMethod == 1)) {
        printf("Redimensionnement activé: %dx%d, méthode: %s\n", width, height, 
            resizeMethod == 0 ? "Plus proche voisin" : "Interpolation bilinéaire");
    } else if (width > 0 || height > 0 || resizeMethod != -1) {
        printf("Paramètres de redimensionnement incomplets ou invalides.\n");
    }

    struct memPartage zone_lecteur = {0};
    
    initMemoirePartageeLecteur(entree,&zone_lecteur);

    struct memPartage zone_ecrivain = {0};
    struct memPartageHeader headerInfos_lecteur = {0};
    struct memPartageHeader headerInfos_ecrivain = {0};
    pthread_mutex_lock(&(zone_lecteur.header->mutex));

    headerInfos_lecteur.canaux = zone_lecteur.header->canaux;
    headerInfos_lecteur.fps = zone_lecteur.header->fps;
    headerInfos_lecteur.hauteur = zone_lecteur.header->hauteur;
    headerInfos_lecteur.largeur = zone_lecteur.header->largeur;
    zone_ecrivain.tailleDonnees = width * height * headerInfos_lecteur.canaux;
    pthread_mutex_unlock(&(zone_lecteur.header->mutex));

    headerInfos_ecrivain = headerInfos_lecteur;
    zone_ecrivain.header = &headerInfos_ecrivain;
    zone_ecrivain.header->hauteur = height;
    zone_ecrivain.header->largeur = width;
    zone_ecrivain.tailleDonnees = height * width * headerInfos_ecrivain.canaux;
    
    initMemoirePartageeEcrivain(sortie,
                            &zone_ecrivain,
                            sizeof(headerInfos_ecrivain)+zone_ecrivain.tailleDonnees,
                            &headerInfos_ecrivain);

    unsigned char* image_data = (unsigned char*)tempsreel_malloc(zone_lecteur.tailleDonnees);
    unsigned char* image_data_resized = (unsigned char*)tempsreel_malloc(zone_ecrivain.tailleDonnees);

    pthread_mutex_lock(&(zone_ecrivain.header->mutex));
    zone_ecrivain.header->frameWriter ++;

    ResizeGrid resized_grid;
    switch (resizeMethod)
    {
    case 0:
        resized_grid = resizeNearestNeighborInit(height, width, headerInfos_lecteur.hauteur, headerInfos_lecteur.largeur);
        break;
    case 1:
        resized_grid = resizeBilinearInit(height, width, headerInfos_lecteur.hauteur, headerInfos_lecteur.largeur);
        break;
    default:
        break;
    }
    
    while(1)
    {        
        evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXLECTURE);
        pthread_mutex_lock(&(zone_lecteur.header->mutex));
        evenementProfilage(&profInfos, ETAT_TRAITEMENT);
        zone_lecteur.header->frameReader++;

        memcpy(image_data, zone_lecteur.data, zone_lecteur.tailleDonnees);
        zone_lecteur.copieCompteur = zone_lecteur.header->frameWriter;
        switch (resizeMethod)
        {
        case 0:
            resizeNearestNeighbor(image_data, zone_lecteur.header->hauteur, zone_lecteur.header->largeur,
                            image_data_resized, height, width,
                            resized_grid, zone_lecteur.header->canaux);
            break;
        case 1:
            resizeBilinear(image_data, zone_lecteur.header->hauteur, zone_lecteur.header->largeur,
                            image_data_resized, height, width,
                            resized_grid, zone_lecteur.header->canaux);
            break;
        
        default:
            memcpy(image_data_resized, image_data, zone_ecrivain.tailleDonnees);
            break;
        }
        pthread_mutex_unlock(&(zone_lecteur.header->mutex));

        evenementProfilage(&profInfos, ETAT_ENPAUSE);
        attenteLecteur(&zone_lecteur);

        evenementProfilage(&profInfos, ETAT_TRAITEMENT);
        memcpy(zone_ecrivain.data, image_data_resized, zone_ecrivain.tailleDonnees);
        zone_ecrivain.copieCompteur = zone_ecrivain.header->frameReader;
        pthread_mutex_unlock(&(zone_ecrivain.header->mutex));

        attenteEcrivain(&zone_ecrivain);

        evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXECRITURE);
        pthread_mutex_lock(&(zone_ecrivain.header->mutex));
        zone_ecrivain.header->frameWriter++;
    }
    
    tempsreel_free(image_data); 

    return 0;
}
