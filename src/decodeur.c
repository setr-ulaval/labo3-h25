/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant le programme de décodage des fichiers ULV
 ******************************************************************************/


// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

#include "jpgd.h"

// Définition de diverses structures pouvant vous être utiles pour la lecture d'un fichier ULV
#define HEADER_SIZE 4
const char header[] = "SETR";

struct videoInfos{
        uint32_t largeur;
        uint32_t hauteur;
        uint32_t canaux;
        uint32_t fps;
};

/******************************************************************************
* FORMAT DU FICHIER VIDEO
* Offset     Taille     Type      Description
* 0          4          char      Header (toujours "SETR" en ASCII)
* 4          4          uint32    Largeur des images du vidéo
* 8          4          uint32    Hauteur des images du vidéo
* 12         4          uint32    Nombre de canaux dans les images
* 16         4          uint32    Nombre d'images par seconde (FPS)
* 20         4          uint32    Taille (en octets) de la première image -> N
* 24         N          char      Contenu de la première image (row-first)
* 24+N       4          uint32    Taille (en octets) de la seconde image -> N2
* 24+N+4     N2         char      Contenu de la seconde image
* 24+N+N2    4          uint32    Taille (en octets) de la troisième image -> N2
* ...                             Toutes les images composant la vidéo, à la suite
*            4          uint32    0 (indique la fin du fichier)
******************************************************************************/


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
    
    // Écrivez le code de décodage et d'envoi sur la zone mémoire partagée ici!
    // N'oubliez pas que vous pouvez utiliser jpgd::decompress_jpeg_image_from_memory()
    // pour décoder une image JPEG contenue dans un buffer!
    // N'oubliez pas également que ce décodeur doit lire les fichiers ULV EN BOUCLE

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
        printf("Mode debug selectionne pour le decodeur\n");
        entree = (char*)"/home/pi/projects/laboratoire3/240p/02_Sintel.ulv";
        sortie = (char*)"/mem1";
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
    
    int fd;

    // Open file
    fd = open(entree, O_RDONLY);
    if (fd == -1) {
        printf("Error opening file %s", strerror(errno));
        return EXIT_FAILURE;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("Erreur lors de la récupération de la taille du fichier");
        close(fd);
        return 1;
    }
    size_t file_size = sb.st_size;

    // MAP_PRIVATE: 
    //  Create a private copy-on-write mapping.  Updates to the mapping are not visible to other processes mapping the  same  file,
    //   and  are  not  carried through to the underlying file.  It is unspecified whether changes made to the file after the mmap()
    //   call are visible in the mapped region.
    // MAP_POPULATE
    //   Populate (prefault) page tables for a mapping.  For a file mapping, this causes read-ahead on the file.  This will help  to
    //   reduce blocking on page faults later.  MAP_POPULATE is supported for private mappings only since Linux 2.6.23.

    void* file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (file_data == MAP_FAILED) {
        perror("Erreur lors du mapping du fichier en mémoire");
        close(fd);
        return 1;
    }

    unsigned char* video_buffer = (unsigned char*)file_data;
    
    if (memcmp(video_buffer, header, HEADER_SIZE) != 0) {
        printf("Invalid file format: expected %.4s, got '%.4s'\n", header, video_buffer);
        close(fd);
        return EXIT_FAILURE;
    }

    video_buffer += HEADER_SIZE;

    struct videoInfos video_info;

    memcpy(&video_info, video_buffer, sizeof(video_info));

    video_buffer += sizeof(video_info);
    unsigned char* iterator = video_buffer;

    size_t frame_size = video_info.largeur * video_info.hauteur * video_info.canaux;

    struct memPartage zone = {0};
    struct memPartageHeader headerInfos = {0};

    headerInfos.canaux = video_info.canaux;
    headerInfos.fps = video_info.fps;
    headerInfos.hauteur = video_info.hauteur;
    headerInfos.largeur = video_info.largeur;

    zone.header = &headerInfos;
    zone.tailleDonnees = frame_size;
    
    initMemoirePartageeEcrivain(sortie,
                            &zone,
                            sizeof(headerInfos)+frame_size,
                            &headerInfos);

    unsigned char* compress_image_data = (unsigned char*)tempsreel_malloc(frame_size);

    uint32_t image_size = UINT32_MAX; 
    size_t image_count = 0;
    char save_ppm_file_path[50]; // Make sure the array is large enough

    pthread_mutex_lock(&(zone.header->mutex));
    zone.header->frameWriter ++;

    while(1)
    { 
        memcpy(&image_size, iterator, sizeof(image_size));
        iterator += sizeof(image_size);
        while (image_size > 0)
        { 
            evenementProfilage(&profInfos, ETAT_TRAITEMENT);
            memcpy(compress_image_data, iterator, image_size);
            iterator += image_size;
            
            unsigned char* image_data = jpgd::decompress_jpeg_image_from_memory(compress_image_data, image_size, (int*)&video_info.largeur, (int*)&video_info.hauteur, (int*)&video_info.canaux, video_info.canaux, 0);
            
            memcpy(zone.data, image_data, zone.tailleDonnees);

            tempsreel_free(image_data); 
            zone.copieCompteur = zone.header->frameReader;
            pthread_mutex_unlock(&(zone.header->mutex));

            evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXECRITURE);
            attenteEcrivain(&zone);
            evenementProfilage(&profInfos, ETAT_TRAITEMENT);

            pthread_mutex_lock(&(zone.header->mutex));
            zone.header->frameWriter++;
            // sprintf(save_ppm_file_path, "/home/pi/projects/laboratoire3/image%d.ppm", image_count);
            // enregistreImage(image_data, video_info.hauteur, video_info.largeur, video_info.canaux, save_ppm_file_path);

            image_count++;
            memcpy(&image_size, iterator, sizeof(image_size));
            iterator += sizeof(image_size);

        }
        iterator = video_buffer;

        image_count = 0;
    }
    
    tempsreel_free(compress_image_data); 
    

    close(fd);

    return 0;
}
