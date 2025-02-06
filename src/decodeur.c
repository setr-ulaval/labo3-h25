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

    printf("%s", argv[2]);

    int fd;
    char buffer[128] = {0};

    // Open file
    fd = open(argv[2], O_RDONLY);
    if (fd == -1) {
        printf("Error opening file %s", strerror(errno));
        return EXIT_FAILURE;
    }


    if (read(fd, buffer, HEADER_SIZE) != HEADER_SIZE) {
        perror("Error reading header");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Check if header matches "SETR"
    if (strncmp(buffer, "SETR", HEADER_SIZE) != 0) {
        printf("Invalid file format: expected 'SETR', got '%.4s'\n", header);
        close(fd);
        return EXIT_FAILURE;
    }

    struct videoInfos video;

    if (read(fd, &video, sizeof(video)) != sizeof(video)) {
        perror("Error reading metadata");
        close(fd);
        return EXIT_FAILURE;
    }

    uint32_t image_size = 0; 

    if (read(fd, &image_size, sizeof(image_size)) != sizeof(image_size)) {
        perror("Error reading image_size");
        close(fd);
        return EXIT_FAILURE;
    }

    unsigned char* compress_image_data = (unsigned char*)tempsreel_malloc((size_t)image_size);

    uint32_t totalRecu = 0; 
    uint32_t octetsTraites = 0; 
    while(totalRecu < image_size){
        octetsTraites = read(fd, compress_image_data + totalRecu, image_size - totalRecu);
        totalRecu += octetsTraites;
    }

    printf("%d", compress_image_data[0]);

    unsigned char* image_data = (unsigned char*)tempsreel_malloc((size_t)(video.largeur * video.hauteur * video.canaux));

    image_data = jpgd::decompress_jpeg_image_from_memory(compress_image_data, image_size, (int*)&video.largeur, (int*)&video.hauteur, (int*)&video.canaux, video.canaux, 0);

    printf("%d", image_data[0]);

    enregistreImage(image_data, video.hauteur, video.largeur, video.canaux, "/home/pi/projects/laboratoire3/image1.ppm");

    tempsreel_free(compress_image_data);
    tempsreel_free(image_data);

    close(fd);

    return 0;
}
