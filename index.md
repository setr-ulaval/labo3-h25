---
title: "Laboratoire 3 : Multiplexage vidéo temps réel sur système embarqué"
---

## 1. Objectifs

Ce travail pratique vise les objectifs suivants :

1. Mettre en place une arborescence de processus temps réel sur un système embarqué;
2. Comprendre la communication interprocessus par mémoire partagée;
3. Se familiariser avec les primitives de synchronisation spécifiques au temps réel, en particulier pour éviter le problème de l'inversion de priorité;
4. Analyser l'impact des différents modes d'ordonnancement offerts par le noyau Linux;
5. Utiliser une projection en mémoire (*memory map*) pour la lecture d'un fichier;
6. Implémenter un système temps réel sur une plateforme limitée en puissance de calcul et comprendre les optimisations à apporter lors de la compilation.

## 2. Présentation du projet

Dans ce laboratoire, votre tâche est d'implémenter un système de multiplexage de vidéos qui pourrait, par exemple, être intégré dans un système de surveillance. Plus formellement, vous devez lire plusieurs vidéos en parallèle, appliquer certains effets sur ces derniers (redimensionnement, filtrage, passage en niveaux de gris, etc.), puis les *composer* dans un affichage permettant de les voir simultanément. Votre système doit être en mesure de prioriser les divers programmes le composant afin d'obtenir une bonne fluidité (mesurée en images par seconde). Ce système fonctionne sur le même principe que les *pipes*, au sens où chaque programme doit pouvoir être chaîné avec les autres. Cependant, les processus ne communiqueront pas en utilisant des *pipes*, peu adaptés à un système temps réel, mais plutôt via des zones mémoire *partagées*. Par ailleurs, l'affichage des vidéos est effectué à un très bas niveau afin d'éviter les délais imprédictibles causés par l'utilisation d'un serveur d'affichage classique.


<img src="img/lab3_banner.jpg" style="width:1000px"/>


## 3. Préparation et outils nécessaires

Ce laboratoire ne nécessite l'installation d'aucune librairie particulière. Comme pour le laboratoire 2, un projet VScode vous est fourni avec des scripts de configuration et une première structure de code. Vous pouvez le récupérer sur le dépôt Git suivant : [https://github.com/setr-ulaval/labo3-h25](https://github.com/setr-ulaval/labo3-h25). Ce dépôt contient aussi des fonctions qui vous sont fournies pour implémenter certaines fonctionnalités du système. De même, plusieurs fichiers d'en-tête (*headers*) vous sont fournis afin de vous guider dans l'implémentation du système. Votre Raspberry Pi Zero devra _être branché sur un écran_ pour pouvoir observer le résultat, puisque nous écrivons directement dans la mémoire du GPU. Utilisez le convertisseur mini-HDMI à HDMI qui vous a été fourni à cet effet.

> **Note** : comme pour le laboratoire 2, vous **devez** modifier les fichiers `.vscode/launch.json` et `syncAndStartGDB.sh` pour y écrire l'adresse de votre Raspberry Pi. Attention, dans le cas du fichier `launch.json`, vous devez le faire à **5** endroits. Assurez-vous également que le dossier `/home/pi/projects/laboratoire3` existe sur votre Raspberry Pi.

> **Note** : le Raspberry Pi désactive par défaut sa sortie HDMI s'il ne détecte pas d'écran au démarrage. Assurez-vous de démarrer votre Raspberry Pi _avec un écran branché_ si vous voulez éviter des problèmes d'affichage.


### 3.1. Fichiers de test

Afin de vous permettre de tester votre approche, nous vous fournissons des [vidéos adaptées au projet](http://wcours.gel.ulaval.ca/GIF3004/lab3_videos_ULV.zip) (attention : taille de 670 Mo!). Ceux-ci sont issus [des projets de la Fondation Blender](https://www.blender.org/features/projects/) et peuvent être distribués librement (en citant leur source). Vous pouvez tester votre projet avec d'autres vidéos, mais assurez-vous que vous possédez les droits pour le faire. Notez que ces vidéos sont encodées de manière spéciale et que vous ne serez pas capables de les ouvrir avec un lecteur classique. Nous reviendrons plus loin sur les spécificités de ce format.

> **Note** : ces vidéos doivent être présentes sur votre Raspberry Pi, dans le dossier contenant vos exécutables (normalement `/home/pi/projects/laboratoire3`). Vous pouvez les transférer manuellement, ou les télécharger (`cd /home/pi/projects/laboratoire3 && wget http://wcours.gel.ulaval.ca/GIF3004/lab3_videos_ULV.zip`) puis les décompresser (`unzip lab3_videos_ULV.zip`) via la ligne de commande.


## 4. Architecture générale

La figure suivante expose l'architecture générale du système.

<img src="img/diag_architecture.png" style="width:1000px"/>

Dans ce diagramme, chaque boîte correspond à un processus distinct. C'est donc dire qu'il y a 10 processus actifs dans le système exposé plus haut. Toutes les communications entre les processus se font via des espaces mémoire partagés. La plupart des processus acceptent à la fois un espace partagé en entrée et en sortie.

> **Définition importante** : pour le reste de l'énoncé, on parle de _lecteur_ lorsque le programme reçoit des données via une zone partagée (ex. le processus `Filtrage`, dans la figure plus haut, est le _lecteur_ de la zone mémoire qu'il partage avec le processus `Redimensionneur`, au-dessus de lui) et _d'écrivain_ lorsque le programme _écrit_ les données sur cette zone partagée. La plupart des programmes, qui possèdent _deux_ zones mémoire partagées, sont donc _à la fois_ des lecteurs (pour la zone partagée qui constitue leur entrée) et des écrivains (pour celle qui est leur sortie). Les processus décodeurs n'utilisent cependant qu'une sortie (et ne sont donc qu'écrivains), puisque leur entrée est constituée d'un fichier vidéo au format ULV. De même, le compositeur possède plusieurs entrées (une pour chaque flux vidéo), mais n'a pas de sortie, puisqu'il affiche directement le résultat à l'écran.

Tous les processus sont des processus avec des contraintes temps réel et la plupart possèdent des dépendances (par exemple, le processus de filtrage de la seconde colonne doit attendre le résultat du processus de redimensionnement, qui nécessite lui-même le résultat du processus de décodage). Il deviendra donc crucial d'assurer un bon ordonnancement de ceux-ci.

### 4.1. Création d'un espace mémoire partagé

Un espace mémoire partagé peut être créé en utilisant la fonction `shm_open`. Cet espace mémoire possédant initialement une taille de zéro, il faut donc l'agrandir en utilisant la fonction `ftruncate`. Finalement, cet espace est, par défaut, vu comme un fichier, ce qui est peu pratique. Pour faciliter son utilisation, nous nous servons de `mmap` qui permet d'utiliser cet espace d'échange comme un pointeur normal. La structure de cette zone mémoire partagée est présentée à la figure suivante.

<img src="img/diag_mem.png" style="width:1000px"/>

Les vingt-quatre premiers octets permettent de stocker une primitive de synchronisation. Les huit octets suivants contiennent des compteurs permettant de déterminer la trame sur laquelle travaillent les processus écrivain et lecteur (et ainsi éviter de traiter deux fois la même trame). Les 6 octets suivants rapportent des informations à propos des images qui seront transmises. Ils sont suivis d'un entier indiquant le nombre d'images par seconde fournies par la source vidéo. Finalement, les données sont présentées sous la forme d'un tableau de *char*, dont la longueur dépend des dimensions de l'image.

### 4.2. Synchronisation

La communication se faisant par espace mémoire partagé, il est important que les différents processus soient synchronisés. Par exemple, lorsque le processus écrit dans la mémoire, le lecteur doit attendre la fin de cette écriture pour lire, au risque de se retrouver avec une image composée de deux trames vidéo différentes. De même, si le processus écrivain ne peut fournir un débit suffisant, le processus lecteur ne doit pas recommencer inutilement le traitement de la même trame.

Le problème de synchronisation est complexifié par le fait qu'il faille gérer *l'inversion de priorité*. Cela signifie que nous devons configurer les mutex avec les attributs leur permettant d'être partagés entre les processus et de tenir compte de potentielles inversions de priorité.

Un mutex peut être référencé par un objet de type `pthread_mutex_t`, qui peut être partagé entre processus. Il est donc possible de le partager facilement dans un espace mémoire partagé. Dans notre cas, il nous permet de bloquer le lecteur tant que l'écrivain n'a pas écrit une trame complète et de bloquer l'écrivain tant que le lecteur n'a pas copié l'entièreté de la trame. Vous êtes libres de concevoir votre propre algorithme de synchronisation adapté à ce problème, mais nous vous fournissons une approche fonctionnelle que vous pouvez utiliser. Celle-ci est illustrée à la figure suivante.
<img src="img/diag_sync.png" style="width:1000px"/>

Dans ce diagramme, l'axe temporel va de haut en bas. On peut distinguer trois sections distinctes : l'initialisation, l'écriture d'une trame vidéo et la lecture d'une trame. Celles-ci sont détaillées dans le texte qui suit.

L'initialisation permet de s'assurer que les processus commencent leur tâche dans un état connu, et ce peu importe l'ordre dans lequel ils ont été lancés. La création de l'espace mémoire partagé est toujours effectuée par le processus écrivain. Le processus lecteur se contente de tenter de l'ouvrir et recommence tant que cette opération échoue. Comme il est également possible que l'écrivain ait créé l'espace partagé mais ne l'ait pas encore agrandi, le lecteur doit également vérifier la taille de l'espace partagé en faisant appel à `fstat`. Si cette taille est plus grande que zéro, alors il attend que le compteur de l'écrivain ait été incrémenté. Le processus écrivain s'assure de son côté d'avoir *créé et acquis* le mutex avant d'incrémenter ce compteur. Lorsque le lecteur constate cette incrémentation, il se met en attente sur le mutex. À ce stade, l'écrivain peut commencer son travail et le lecteur sait qu'un écrivain actif est présent, nous passons donc à la boucle principale.

L'écrivain possédant le mutex, il peut modifier la zone mémoire partagée. Lorsqu'il a terminé, il conserve une copie de la valeur du compteur du lecteur puis libère le mutex. Le lecteur acquiert ce dernier et incrémente son compteur. Il peut alors lire le contenu de l'espace partagé sans risque de le voir être modifié. Lorsqu'il a terminé sa lecture, il copie la valeur du compteur de l'écrivain dans une variable locale puis libère le mutex. Du côté de l'écrivain, la première chose à faire lorsqu'il libère le mutex est de vérifier si le compteur du lecteur a été incrémenté : si ce n'est pas le cas, cela signifie que le lecteur n'a pas encore pu acquérir le mutex et donc que même si ce dernier est libre, il ne sert à rien de l'acquérir. L'écrivain boucle donc tant que le lecteur n'incrémente pas son compteur. Lorsque c'est le cas, l'écrivain sait que le lecteur possède le mutex et se met en attente sur ce dernier. Lorsque le lecteur a terminé et que l'écrivain récupère le mutex, il incrémente son compteur, ce qui permet au lecteur de savoir qu'il peut se remettre en attente sur le mutex, et le cycle recommence.

Le fichier *commMemoirePartagee.h* contient des déclarations de structure et de fonctions qui pourront vous être utiles pour implémenter l'algorithme de synchronisation. Créez vos fonctions dans un fichier nommé *commMemoirePartagee.c*. N'oubliez pas d'être générique : tous les programmes que vous écrirez ont besoin de ces primitives de synchronisation, il est donc souhaitable d'utiliser toujours les mêmes fonctions!

**Considérations importantes** : un processus temps réel ne peut être interrompu par le noyau (sauf cas spécifiques, selon l'ordonnanceur utilisé). Par conséquent, lorsque votre processus ne peut s'exécuter, vous *devez* redonner la main aux autres processus. Cela peut être fait de trois manières. Premièrement, en utilisant un appel système qui permet explicitement à l'ordonnanceur de vous interrompre (l'attente sur un mutex fait par exemple partie de ces opérations). Deuxièmement, en appelant la fonction `sched_yield` pour indiquer que vous acceptez volontairement de laisser le CPU à d'autres processus. Troisièment, en vous mettant volontairement en sommeil en utilisant `usleep`. La différence entre ces deux dernières approches est que `sched_yield` n'est qu'une **indication** donnée au noyau, qui peut très bien vous réveiller immédiatement sans passer à main à un autre processus s'il considère que vous êtes toujours le processus le plus prioritaire, alors que `usleep` force le noyau à ne pas vous réveiller pour la durée approximative que vous avez spécifiée. En d'autres termes, n'utilisez `sched_yield` que dans les cas où vous _pouvez_ laisser la main mais que vous auriez tout de même du travail à faire, et `usleep` lorsque vous n'avez rien à faire pour une période de temps approximativement connue (par exemple le temps avant la prochaine trame du vidéo).

Notez finalement que la plupart des programmes sont *à la fois* des lecteurs et des écrivains! Ceux-ci auront donc à gérer des mutex des deux "côtés"!

### 4.3. Gestion de la mémoire

Dans un programme standard, l'allocation mémoire peut se faire dynamiquement en utilisant par exemple `malloc`. Toutefois, dans un programme temps réel, cette fonction introduit des délais variables indésirables. Une fois dans la section critique d'un programme temps réel, *il faut donc se passer de malloc*. Par ailleurs, le noyau peut techniquement toujours *déplacer* des zones mémoire allouées, que ce soit ailleurs en mémoire ou sur le disque, ce qui n'est encore une fois pas souhaitable dans un cadre temps réel. Ces problèmes peuvent être réglés de la manière suivante :

1. Lors de l'initialisation du programme, lorsqu'il n'a pas encore commencé sa section critique, allouez dynamiquement (en utilisant *malloc*) une zone mémoire suffisamment grande pour contenir quatre ou cinq fois la taille des images que votre programme doit traiter. Vous pouvez par la suite remplacer les appels à *malloc* par une fonction personnalisée, par exemple *tempsreel_malloc*, qui va utiliser cette zone mémoire comme un bassin de mémoire (*memory pool*). Lorsqu'une partie de votre programme requiert une allocation mémoire, votre fonction va chercher s'il existe un emplacement libre dans ce bassin et la lui retourner. De même, lorsque la mémoire est libérée (par exemple en utilisant *tempsreel_free*), la fonction va indiquer que le bloc est maintenant libre.
2. Afin d'assurer que vos allocations mémoire vont rester en mémoire, utilisez la fonction `mlock` ou `mlockall`. Ces fonctions permettent de « fixer » un buffer en mémoire, au sens où le système d'exploitation garantit que leur emplacement ne changera pas tant que vous ne l'accepterez pas explicitement. Par défaut, un programme ne peut « fixer » qu'un nombre limité d'octets. Vous pouvez utiliser la fonction *setrlimit* avec l'argument *RLIMIT_MEMLOCK* pour modifier cette valeur. Toutefois, votre programme devra alors être exécuté en tant que *root* (ou en utilisant `sudo`).

Des en-têtes implémentant une interface possible pour ce gestionnaire de mémoire simple vous est fourni dans *allocateurMemoire.h*. Implémentez vos fonctions dans un fichier nommé *allocateurMemoire.c*.


### 4.4. Arguments des programmes

Tous les programmes possèdent une interface ligne de commandes similaire, soit :

```sh
./nomduprogramme [options] flux_entree flux_sortie
```

*flux_entree* et *flux_sortie* constituent respectivement les identifiants des zones mémoire partagées en entrée et en sortie. Les options sont quant à elles différentes selon les programmes. Parmi celles *devant être disponibles pour tous les programmes*, nous retrouvons :

* "-s" qui détermine le type d'ordonnancement voulu. Il peut prendre la valeur *NORT* (ordonnancement normal), *RR* (ordonnancement temps réel avec préemption), *FIFO* (ordonnancement temps réel sans préemption) ou *DEADLINE* (ordonnancement de type plus proche échéance). Toute autre valeur est invalide. Par défaut, la valeur est "NORT". Dans le cas de *NORT*, vous n'avez aucune opération à faire, puisque cette option correspond au mode par défaut de l'ordonnanceur. Dans les autres cas, vous devez changer le mode de l'ordonnanceur (voir la section 4.5).

* "-d" qui, dans le cas de l'ordonnanceur *DEADLINE*, permet de fournir les valeurs en millisecondes pour runtime, deadline et period, respectivement, séparés par des virgules. Par exemple, `-d 10,20,25` indique un runtime de 10 ms, un deadline de 20 ms et une période de 25 ms. Si l'ordonnanceur n'est pas de type *DEADLINE* (autrement dit, l'option -s ne contient pas DEADLINE), alors vous pouvez ignorer ces valeurs qui n'auront pas d'effet.

Dans tous les cas, **vous pouvez assumer que les arguments donnés à vos programmes seront toujours valides** : vous n'avez pas à effectuer de vérification à cet effet. Toutefois, l'ordre des options n'est pas garanti (celui des flux_entree / flux_sortie l'est quant à lui). Nous vous recommandons fortement d'utiliser une librairie comme getopt pour vous aider dans l'analyse des arguments. 

> **Remarque** : le fichier `src/convertisseurgris.c` contient une implémentation complète de l'analyse des arguments pour cet exécutable. Vous pouvez vous en inspirer pour les autres exécutables.

Finalement, vous devez implémenter un mode de lancement spécial servant au débogage :

```sh
./nomduprogramme --debug
```

Dans ce mode, vous devez attribuer des valeurs aux différents paramètres. Vous êtes libres de choisir ces paramètres et ils peuvent être spécifiques à votre installation, mais assurez-vous qu'ils permettent de lancer les programmes sans erreur, puisque c'est ainsi que le mode de débogage de VScode lancera tous vos programmes.

### 4.5. Changement de mode du scheduler

Si l'ordonnancement demandé n'est pas *NORT*, vous devez utiliser *sched_setscheduler* ou *sched_setattr*, fournis dans le header `schedsupp.h`, pour choisir le mode d'ordonnancement demandé. Notez que les modes temps réel nécessitent les droits d'administrateur pour pouvoir être utilisés, vous devez donc lancer votre programme avec *sudo* (c'est déjà fait dans les scripts de lancement). Voyez [la page de manuel de sched_setattr](https://man7.org/linux/man-pages/man2/sched_setattr.2.html) et [une description plus complète du mode deadline](https://man7.org/linux/man-pages/man7/sched.7.html) pour vous aider à comprendre comment faire ce changement d'ordonnanceur. C'est aussi cette dernière page qui décrit ce que signifient _runtime_, _deadline_ et _period_.

> **Attention** : initialisez toutes les valeurs de la structure `sched_attr` à 0, même celles que vous n'utilisez pas! Par ailleurs, portez attention à _l'unité_ des champs deadline, runtime et period.

> **Attention** : validez que le changement d'ordonnanceur s'effectue avec succès en vérifiant que la valeur de retour de `sched_setattr()` est bien 0. Toute autre valeur indique une erreur que vous pouvez déterminer en affichant le retour de la fonction `strerror(errno)`.

## 5. Modules à implémenter

Vous avez cinq modules (programmes) à implémenter :

* **Décodeur** (fichier *decodeur.c*) : lit un fichier vidéo, décode les trames qu'il contient et les écrit une par une dans un espace mémoire partagé.
* **Compositeur** (fichier *compositeur.c*) : reçoit de un à quatre espaces mémoire et affiche les vidéos en simultané sur l'écran.
* **Redimensionneur** (fichier *redimensionneur.c*)  : redimensionne les images qu'il reçoit en entrée, que ce soit vers une taille plus petite ou plus grande que celle courante.
* **Filtreur** (fichier *filtreur.c*)  : applique un filtre passe-bas ou passe-haut sur l'image.
* **Convertisseur niveaux de gris** (fichier *convertisseurgris.c*) : convertit une image RGB (3 canaux) en niveaux de gris (1 canal).

Les sous-sections suivantes détaillent chacun de ces programmes.

### 5.1. Décodeur

Le décodeur est responsable de la lecture d'un fichier vidéo. Son premier argument n'est donc pas l'identifiant d'une zone mémoire partagée, mais plutôt un fichier contenant le vidéo. Les formats vidéo (mpeg, h264, hevc, etc.) étant difficiles à gérer en temps réel et sans librairies externes, vous utiliserez un format vidéo spécialement créé pour ce laboratoire, le format *ULV* (*Université Laval Vidéo*). Ce format est présenté dans la figure suivante.

<img src="img/diag_formatvideo.png" style="width:800px"/>

Tout fichier ULV commence donc par 4 octets qui correspondent aux lettres S, E, T et R en ASCII. Cela permet au programme de s'assurer que le fichier qu'il tente de lire est du bon format. Par la suite, 4 octets sont utilisés pour indiquer la largeur des images du vidéo, puis 4 octets pour leur hauteur et 4 octets pour le nombre de canaux. Finalement, 4 octets permettent d'indiquer le nombre d'images par seconde du vidéo (usuellement 30, mais ce nombre peut varier). Les trames du vidéo sont par la suite enregistrées une par une. Pour chaque trame, un entier non signé de 32 bits indique la taille, suivi d'un tableau d'octets. Ce tableau *ne contient pas directement l'image, mais son encodage JPEG*. Vous devez donc *décompresser* l'image, nous vous fournissons une fonction le faisant pour vous dans le fichier *jpgd.h* (voyez la fonction `decompress_jpeg_image_from_memory` dans le fichier d'en-tête et les explications que nous y avons ajoutées). Les images sont ainsi encodées à la suite, jusqu'à la dernière qui est suivie d'un entier de 4 octets contenant 0. Cette valeur n'étant pas une taille valide pour une image, vous savez alors que vous avez atteint la fin du fichier. *Vous devez alors recommencer la lecture au début du fichier, en boucle.*

Afin de vous permettre d'expérimenter avec d'autres vidéos et de voir le résultat attendu, nous vous fournissons deux scripts Python, disponible sur [le dépôt Git](https://github.com/setr-ulaval/labo3-h25), capables de convertir (videoConverter.py) et de lire (videoReader.py) les vidéos au format ULV. Ceux-ci requièrent scipy et OpenCV pour fonctionner. Notez que vous n'êtes pas forcés de les utiliser et qu'aucune fonctionnalité du laboratoire ne dépend du bon fonctionnement de ces scripts, qui vous sont fournis à titre d'exemple uniquement.

**Note importante :** l'affichage du Raspberry Pi Zero gère les images en mode BGR et non RGB (les canaux rouge et bleu sont inversés). Par conséquent, le format ULV enregistre ces images en inversant les canaux rouge et bleu. Les scripts fournis tiennent compte de cette inversion et restituent l'image en réinversant les canaux, mais ne soyez pas surpris par cette inversion de canal si vous essayez d'afficher une trame par vous-mêmes.

Tel que mentionné plus haut, le programme décodeur est lancé en spécifiant un fichier comme premier argument au lieu d'une zone mémoire partagée :

```sh
./decodeur [options] fichier_entree flux_sortie
```

Les fonctions d'entrée/sortie ne font pas bon ménage avec le temps réel. En effet, la lecture sur un périphérique de stockage induit des délais aléatoires qui ne sont pas souhaitables. Par conséquent, il faut s'assurer que le fichier soit entièrement copié en mémoire RAM avant de commencer. Pour ce faire, utilisez la fonction `mmap` sur le descripteur de fichier, accompagnée de l'argument *MAP_POPULATE*. Vous pourrez ensuite lire le fichier comme s'il s'agissait d'un tableau en mémoire.

Dans le cadre de ce laboratoire, vous pouvez supposer que la somme totale des tailles des fichiers vidéo ne dépassera jamais la taille de la RAM. En d'autres termes, copier les fichiers dans la RAM lors de l'initialisation du décodeur ne devrait jamais causer une erreur par manque de mémoire.

### 5.2. Compositeur

Le compositeur est responsable de l'affichage des flux vidéos. Il doit pouvoir accepter de 1 à 4 flux vidéos distincts, mais possédant les mêmes dimensions, qui seront affichés comme suit (le grand rectangle noir représente l'écran).

<img src="img/comp_1.png" style="width:250px"/>
<img src="img/comp_2.png" style="width:250px"/>
<img src="img/comp_3.png" style="width:250px"/>
<img src="img/comp_4.png" style="width:250px"/>

La façon de l'appeler sur la ligne de commande diffère quelque peu des autres programmes :

```sh
./compositeur [options] flux_entree1 [flux_entree2] [flux_entree3] [flux_entree4]
```

où flux_entreeN correspond au flux vidéo #N dans les schémas ci-dessous. Le premier flux doit être précisé, les flux vidéo 2 à 4 sont optionnels. Par exemple :

```sh
./compositeur -s RR /flux1_mem /flux2_mem
```

exécutera le compositeur en mode temps réel avec préemption, en affichant deux flux vidéo en simultané.

Considérant l'aspect temps réel de ce laboratoire, l'utilisation d'une interface graphique est à proscrire. Par conséquent, il nous faut donc utiliser des primitives d'affichage de plus bas niveau. Dans ce laboratoire, l'affichage se fait en écrivant directement dans le *framebuffer* du système. Le *framebuffer* est un espace mémoire utilisé pour communiquer directement avec la carte graphique. C'est donc dire que nous écrirons directement la valeur de chacun des pixels dans une mémoire lue par la carte vidéo. Cette interface étant de très bas niveau, aucune abstraction n'est fournie pour, par exemple créer des fenêtres ou des zones dans l'écran. Sa configuration peut également être malaisée. Afin de faciliter votre tâche, nous vous fournissons un fichier nommé *compositeur.c* qui contient déjà plusieurs fonctions d'affichage. Vous devez le compléter avec le code lui permettant d'aller récupérer les images dans une zone mémoire partagée. Voyez les commentaires inclus dans ce fichier pour plus de détails.

> **Remarque** : lors de l'affichage des vidéos avec le compositeur, il se peut qu'un caractère clignorant apparaisse en bas à gauche de la première source vidéo. Cet artefact est dû au fait que nous ne désactivons pas complètement le terminal, et n'est pas important. Il ne sera pas pris en compte lors de l'évaluation. Toutefois, toute autre dégradation de la qualité de l'image indique souvent un problème.

#### 5.2.1. Nombre d'images par seconde

Votre programme doit afficher les images aussi vite que possible, mais en respectant la vitesse _maximale_ donnée dans le champ *Nombre d'images par seconde* du fichier ULV. Par exemple, même si vous _pourriez_ afficher 40 images par seconde, vous devez vous limiter à 30 si c'est la limite indiquée dans le fichier, sinon la vidéo passera en accéléré!

#### 5.2.2. Écriture des statistiques de fluidité

En plus d'afficher les vidéos, le compositeur doit également écrire un fichier nommé *stats.txt*. Ce fichier doit être vidé à chaque nouvelle exécution (il suffit de l'ouvrir avec l'option "w" passée à `fopen()` pour l'effacer) et doit avoir le contenu suivant:

```
[TEMPS_ECOULE] Entree 1: moy=MOYENNE_FPS fps, max=TEMPS_MAXIMUM ms | Entree 2: moy=MOYENNE_FPS fps, max=TEMPS_MAXIMUM ms | Entree 3: moy=MOYENNE_FPS fps, max=TEMPS_MAXIMUM ms | Entree 4: moy=MOYENNE_FPS fps, max=TEMPS_MAXIMUM ms
```

où *TEMPS_ECOULE* est le temps écoulé depuis le lancement du compositeur, *MOYENNE_FPS* est le nombre moyen d'images par seconde durant les **5 dernières secondes** et *TEMPS_MAXIMUM* le délai **maximal** entre 2 trames affichées durant les 5 dernières secondes. *TEMPS_ECOULE*, *MOYENNE_FPS* et *TEMPS_MAXIMUM* doivent être affichés avec *un chiffre après la virgule*. À toutes les 5 secondes (environ), votre compositeur doit ajouter une nouvelle ligne au fichier contenant ces informations. Notez que vous ne devez afficher les entrées que si elles sont actives. Par exemple, voici une sortie typique pour une configuration à 3 entrées:

```
[5.0] Entree 1: moy=20.1 fps, max=116.5 ms | Entree 2: moy=21.3 fps, max=89.1 ms | Entree 3: moy=21.0 fps, max=89.1 ms | 
[10.0] Entree 1: moy=20.2 fps, max=156.5 ms | Entree 2: moy=20.6 fps, max=109.7 ms | Entree 3: moy=20.5 fps, max=88.5 ms | 
[15.1] Entree 1: moy=20.6 fps, max=117.1 ms | Entree 2: moy=20.8 fps, max=108.8 ms | Entree 3: moy=20.8 fps, max=110.0 ms | 
[20.0] Entree 1: moy=20.2 fps, max=118.1 ms | Entree 2: moy=20.3 fps, max=138.4 ms | Entree 3: moy=19.9 fps, max=131.8 ms |
```

> **Conseil** : pour éviter que le _buffering_ ne vous joue des tours si votre programme se termine inopinément, vous pouvez utiliser `setbuf` comme dans le laboratoire 1 pour désactiver tout _buffering_ :

```
FILE *fstats = fopen("stats.txt", "w");
setbuf(fstats, NULL);
```



### 5.3. Redimensionneur

Le redimensionneur prend en entrée une image d'une taille arbitraire et retourne en sortie la même image, mais redimensionnée aux dimensions demandées. Le redimensionnement peut être fait par une méthode des plus proches voisins (rapide, mais peu précise) ou par interpolation bilinéaire (plus lente, mais produisant de meilleurs résultats). Dans les deux cas, les fonctions opérant ce redimensionnement vous sont fournies dans le fichier *utils.h*.

En plus des options normales, ce processus requiert "-w" et "-h" (largeur et hauteur des images en sortie) et "-r", qui peut prendre les valeurs 0 ou 1, 0 correspondant à un redimensionnement au plus proche voisin et 1 à une interpolation linéaire.

```sh
./redimensionneur [options] flux_entree flux_sortie
```

### 5.4. Filtreur

Le filtreur filtre l'image qu'il reçoit en entrée avec un filtre passe-bas ou passe-haut. Il écrit l'image résultante dans son espace partagé en sortie. Les fonctions opérant ces opérations de filtrage vous sont fournies dans le fichier *utils.h*. Sa ligne de commande accepte un paramètre supplémentaire "-f", qui détermine le type du filtre et peut valoir soit 0 pour un filtre passe-bas, soit 1 pour un filtre passe-haut :

```sh
./filtreur [options] flux_entree flux_sortie
```

### 5.5. Convertisseur vers niveaux de gris

Comme son nom l'indique, ce programme convertit son entrée RGB en niveaux de gris. La balance de chaque canal est effectuée selon l'espace de couleur CIE 1931. La fonction opérant cette conversion est déjà codée pour vous et disponible dans le fichier *utils.h*. Ce programme ne possède pas d'arguments supplémentaires sur sa ligne de commande.

```sh
./convertisseur [options] flux_entree flux_sortie
```

Tel que mentionné plus haut, l'analyse des arguments de ce programme en particulier est déjà codée pour vous, voyez le code pour comprendre comment l'utiliser.

## 6. Lancement des programmes

Tel que mentionné plus haut, VScode lancera automatiquement vos programmes avec l'unique argument `--debug`. Assurez-vous donc qu'il correspond à une configuration valide. Afin de vous permettre de déboguer plusieurs programmes simultanément sans avoir à lancer plusieurs instances de VSCode, une configuration de *débogage multiple* a été mise en place. Pour y accéder, sélectionnez l'onglet *Débogage* dans les icônes de gauche (4e icône à partir du haut). Vous verrez alors un menu vous permettant de lancer le débogage pour chacun des programmes demandés.

<img src="img/vscode_debug1.png" style="width:350px"/>

Notez que vous pouvez lancer **plusieurs** sessions de débogage simultanément. Lorsque vous le faites, un menu de sélection supplémentaire apparaîtra à droite de la barre d'outils de débogage; vous pourrez sélectionner ainsi le programme que vous souhaitez contrôler.

> **Note** : comme pour le précédent laboratoire, le débogueur peut bloquer sur une exception avant même d'avoir commencé à exécuter votre programme. Assurez-vous donc de **toujours** inclure un point d'arrêt au début de votre fonction `main()`.

Le débogage, où un programme externe (le débogueur) peut bloquer à volonté un autre programme, s'accorde mal avec la notion de temps réel. Utilisez donc le débogueur pour tester vos programmes uniquement en mode normal (sans temps réel).

### 6.1. Mode de compilation

Pour la première fois du cours, la performance des programmes joue un rôle très important dans les résultats du laboratoire. En effet, obtenir une bonne fluidité sur un système aussi limité que le Raspberry Pi Zero présuppose une implémentation très efficace. Le compilateur peut vous aider à cet effet. Pour cela, vous devez le configurer en mode **Release**. Dans la palette de commandes de VScode, saisissez *CMake : select variant*, puis *Release* (vous pouvez également sélectionner la configuration dans la barre de statut, à gauche). Recompilez ensuite vos programmes.

Attention toutefois, cette performance accrue a un prix : *vous ne serez pas en mesure de déboguer dans ce mode*, puisque le compilateur peut modifier votre code de manière importante afin de l'optimiser. Assurez-vous donc que vos programmes fonctionnent sans erreur avant d'utiliser ce mode de compilation. Il est impossible qu'un programme ne fonctionnant pas en mode *Debug* fonctionne bien en mode *Release*, au contraire : les optimisations du compilateur peuvent parfois faire ressortir de nouveaux bogues!

### 6.2. Exécution de scénarios

Une fois que vos programmes sont prêts à être utilisés, vous pouvez commencer vos expérimentations utilisant différents *scénarios* (c'est-à-dire différentes configurations plus ou moins demandantes pour le Raspberry Pi). Ces scénarios sont des scripts Bash; nous vous en présentons un certain nombre (11, pour être plus précis) dans le répertoire `configs` du dépôt. Ils sont, de manière générale, de difficulté croissante. Ces scripts doivent être présents *sur le Raspberry Pi*, dans le même répertoire que celui contenant les exécutables de vos programmes (normalement, `/home/pi/projects/laboratoire3`). Vous pouvez lancer une configuration donnée en utilisant la commande `bash nom_du_script.bash`.

Notons que pour arrêter tous les programmes, vous devez :

1. Couper le compositeur (par exemple en utilisant Ctrl+C)
2. Terminer toutes les tâches d'arrière-plan (utilisez par exemple `sudo killall sudo`, puisque tous les exécutables sont lancés en mode `sudo` pour leur permettre de changer de mode d'ordonnancement)

Ces scripts **doivent** être exécutés sur le Raspberry Pi, mais n'ont pas forcément à l'être sur un terminal local. En d'autres termes, vous pouvez lancer les programmes à partir d'une session à distance (SSH) et voir le résultat s'afficher à l'écran connecté au Raspberry Pi.

Ces scripts utilisent toujours plus d'un programme à la fois. Afin de vous éviter d'avoir à les synchroniser un par un (en lançant successivement leur débogage), nous vous fournissons une configuration spéciale dans VSCode permettant de synchroniser tous les fichiers d'un seul coup. Pour ce faire, allez dans la palette de commandes et écrivez "Tâches: Exécuter la tâche", puis sélectionnez *toutSynchroniser*. Une fois cette tâche terminée, tous les exécutables seront à jour sur le Raspberry Pi.

### 6.3. Implémentation et débogage

Vous avez au total *deux* librairies partagées (`allocateurMemoire` et `commMemoirePartagee`) et *cinq* programmes (`decodeur`, `compositeur`, `convertisseurgris`, `redimensionneur` et `filtreur`) à implémenter. Notez toutefois qu'une bonne partie de la logique de ces programmes (lecture et écriture dans une zone mémoire partagée, changement de mode d'ordonnancement, etc.) est très similaire voire identique et que les fonctions effectuant les transformations (redimensionnement, conversion en niveaux de gris et filtrage) sont déjà implémentées pour vous dans `utils.c`, vous n'avez qu'à les appeler correctement.

Nous vous conseillons de développer vos programmes dans cet ordre:

1. Implémentez `allocateurMemoire`, mais en utilisant, de manière temporaire, directement `malloc()` et `free()` (c'est-à-dire que `tempsreel_malloc()` ne fait que retourner le résultat de `malloc()` sans rien faire d'autre). Ce n'est **pas** ce qui est demandé, mais ça vous permettra de développer vos programmes sans risquer des problèmes dus à un allocateur mémoire défaillant.
2. Implémentez le `decodeur`. Testez-le en utilisant le `compositeur` fourni. Au besoin, vous pouvez utiliser la fonction `enregistreImage()` dans `utils` pour enregistrer une image au format PPM, que vous pouvez par la suite télécharger sur votre ordinateur et ouvrir avec un lecteur d'image standard.
3. Implémentez le `convertisseurgris`. Son implémentation est très simple (il suffit d'appeler `convertToGray()`), mais pour que cela fonctionne, vous devrez également implémenter `commMemoirePartagee`. Finalement, vous devrez écrire le code permettant à `convertisseurgris` de lire sur ce même espace mémoire partagé. Utilisez encore une fois `enregistreImage()` et/ou les programmes solution fournis pour valider que le code de `convertisseurgris` fonctionne correctement.
4. Implémentez le `compositeur`. Son code de lecture sur l'espace mémoire partagé sera très similaire à celui de `convertisseurgris`, à la différence près que `compositeur` ne doit pas bloquer si une source n'est pas disponible, car il doit gérer jusqu'à 4 sources en parallèle! Assurez-vous que vous êtes capables _d'afficher_ une vidéo (vous pouvez utiliser la configuration 01_sourceUnique pour vous tester) à une vitesse correcte (on ne doit pas dépasser le nombre d'images par seconde identifié dans le fichier ULV!). Vérifiez également que vous êtes capables de produire le fichier `stats.txt` tel que demandé à la section 5.2.1.
5. Implémentez le `redimensionneur` et le `filtreur`. Leur code devrait être très similaire à celui de `convertisseurgris`, à l'exception de l'analyse des arguments sur la ligne de commande et de la fonction de traitement à lancer. À ce stade, vous devriez être capables d'exécuter les configurations 01 à 08 sans erreur.
6. Remplacez l'implémentation factice d'`allocateurMemoire` par une implémentation valide telle que décrite à la section 4.3. Tous vos programmes devraient également utiliser `tempsreel_malloc()` / `tempsreel_free()`, jamais `malloc()` et `free()`. Revalidez les configurations 01 à 08 pour confirmer que votre allocateur fonctionne correctement.
7. Implémentez les différents modes d'ordonnancement (RT, FIFO et DEADLINE) et assurez-vous que votre code change _réellement_ l'ordonnancement utilisé (le retour de la fonction `sched_setattr()` devrait être `0`, si c'est une valeur négative, c'est que ça ne marche pas). Testez les configurations 09, 10 et 11 qui utilisent ces modes.
8. Optimisez vos programmes pour maximiser la performance des programmes (sans en affecter le résultat, bien entendu).

#### 6.3.1. Exécutables fournis

Afin de faciliter votre débogage, nous vous fournissons, comme pour le laboratoire 2, les **exécutables binaires (compilés) pour chaque programme** (donc 5 exécutables au total). Ces exécutables constituent la solution du laboratoire et sont présents dans le dossier *executables* du dépôt Git. Vous pouvez donc, par exemple, tester votre programme decodeur en utilisant la solution du compositeur, et vice-versa. Évidemment, ces binaires ne peuvent être remis pour l'évaluation : vous devez implémenter vos propres programmes! Ces programmes peuvent être vous permettre de tester les différents scénarios afin de voir ce que devrait être le résultat final en terme d'aspect graphique et de fluidité.

Pour les utiliser, copiez les exécutables que vous voulez utiliser comme solutionnaire dans le répertoire `src/build`. Exécutez par la suite la tâche *toutSynchroniser*, tel qu'expliqué à la section 6.2. Par la suite, *recompilez* puis lancez le programme que vous voulez déboguer en vous assurant qu'il est configuré pour utiliser les bonnes zones mémoire d'entrée/sortie.

> **Attention** : bien que les programmes fournis soient *corrects* (au sens où ils respectent l'énoncé du laboratoire), ils ne sont pas infaillibles et résistants à toute requête incorrecte. Envoyer des données erronées à ces programmes _peut_ conduire à un plantage ou un blocage du programme. Par exemple, si vous ne relâchez jamais le mutex de synchronisation, les programmes fournis resteront bloqués. De même, si vous envoyez une taille d'image invalide dans la zone mémoire partagée, il est probable que cela produise une erreur de segmentation.

## 7. Temps réel, profilage et optimisation

### 7.1. Essais des différents modes de l'ordonnanceur

Une fois les différents programmes implémentés, vous pouvez passer au test de ceux-ci. Commencez par lancer ces programmes en mode normal (non temps réel) et observez les résultats en utilisant 1, 2, 3 ou 4 vidéos de différentes tailles. Analysez le fichier *stats.txt* pour déterminer les sources posant le plus problème.

Par la suite, lancez ces programmes en temps réel, avec l'ordonnanceur de base (SCHED_RR). Observez-vous une différence?

Finalement, pour la dernière étape, utilisez l'ordonnanceur SCHED_DEADLINE. Pour cela, vous devrez d'abord mesurer le temps d'exécution estimé pour chaque programme selon ses paramètres d'entrée. Ne cherchez pas à être trop précis et ne passez pas trop de temps à mesurer ces temps, mais assurez-vous d'avoir des estimations fiables. Vous pourrez ensuite passer ces valeurs à l'ordonnanceur et observer la différence par rapport aux autres modes d'ordonnancement (en mode deadline, la "priorité" de votre processus devrait être de -101). Dans quels cas cela fait-il le plus de différence?

**Note importante** : certaines combinaisons de vidéos requièrent tout simplement trop de temps pour être traitées en temps réel par le Raspberry Pi (c'est par exemple le cas si vous ouvrez une vidéo en 480p et appliquez un filtre et un redimensionnement avec interpolation). L'ordonnanceur ne peut pas faire de miracle, et si le temps CPU demandé par un groupe de programme excède la capacité totale de l'ordinateur, il n'y a rien à faire. Toutefois, remarquez ce qui se passe lorsque, *en même temps* que ces tâches trop longues, vous lancez une autre tâche qui, elle, pourrait s'exécuter dans les temps. Le choix du mode d'ordonnancement améliore-t-il sa fluidité?

### 7.2. Visualisation de l'état des programmes au fil du temps

Afin de vous permettre de mieux visualiser et comprendre l'état de chaque programme au fil du temps, nous vous fournissons du code permettant une forme de _profilage_ de vos exécutables. Ce code génère des fichiers texte (tous nommés `profilage-NomDuProgramme-PID`) contenant les temps précis correspondant à des **changements d'état** de vos programmes. Ces états sont au nombre de cinq :

1. L'initialisation (tout ce qui se passe avant que vous ne commenciez la boucle critique du programme);
2. L'attente en tant que lecteur (correspondant à la fonction `attenteLecteur`, donc peut être une attente sur le mutex ou une attente de l'incrémentation du compteurEcrivain);
3. Le traitement à proprement parler (autrement dit, ce que fait réellement le programme, comme redimensionner ou convertir en niveaux de gris);
4. L'attente en tant qu'écrivain (correspondant à la fonction `attenteEcrivain`, donc peut être une attente sur le mutex ou une attente de l'incrémentation du compteurLecteur);
5. La mise en sommeil volontaire (lorsqu'un programme appelle `usleep`, par exemple).

Une fois ces fichiers générés, vous pouvez vous servir d'un script Python (dans `src/creerProfilageImages.py`) pour générer des figures semblables à celle-ci (dans ce cas-ci, le scénario 08) :

<img src="img/graph.png" style="width:1000px"/>

Cela vous permet de voir rapidement quel programme passe plus de temps à attendre et pour quelle raison (entrée ou sortie), ou quel programme obtient la part du lion de l'utilisation CPU. Ces figures nous permettront également de plus facilement valider votre implémentation.

> **Note** : si vous voulez exécuter le script `creerProfilageImages.py` sur la VM fournie, vous devrez d'abord installer numpy et matplotlib en utilisant la commande `pip3.11 install numpy matplotlib` dans un terminal.

> **Note** : l'apparence de la figure de profilage peut différer de celle montrée plus haut. Tout dépend de votre implémentation, il y a plusieurs façons valides de réaliser le laboratoire. Ne cherchez pas à répliquer _exactement_ la figure montrée plus haut pour le scénario 08. Vous devez toutefois comprendre la relation entre ce que vous faites dans votre code et ce qui se retrouve dans la figure.

#### 7.2.1. Comment activer le profilage

L'initialisation du profileur est déjà faite pour vous dans les fichiers de code fournis. Laissez la au début de la fonction `main()` de chaque programme (écrivez votre code _après_). **Vous devez vous-même ajouter des appels à `evenementProfilage`**. Le premier argument de `evenementProfilage` sera toujours `&profInfos`. Quant au second, il dépend d'où vous ajoutez l'appel :

- Juste **avant** l'appel à `attenteLecteur`, appelez `evenementProfilage` avec le 2e argument valant `ETAT_ATTENTE_MUTEXLECTURE` (sauf pour `decodeur` qui n'attend pas sur un mutex pour son entrée)
- Juste **avant** l'appel à `attenteEcrivain`, appelez `evenementProfilage` avec le 2e argument valant `ETAT_ATTENTE_MUTEXECRITURE` (sauf pour `compositeur` qui n'a pas d'écriture à faire)
- Juste **après** l'appel à `attenteLecteur`, ou au début du code de décodage pour `decodeur`, appelez `evenementProfilage` avec le 2e argument valant `ETAT_TRAITEMENT`
- Juste **avant** l'appel à `usleep`, `sched_yield`, ou une autre fonction où votre programme redonne volontairement la main, appelez `evenementProfilage` avec le 2e argument valant `ETAT_PAUSE`

#### 7.2.2. Utilisation du profilage

La figure obtenue peut être utilisée pour mieux analyser et déboguer votre code. Par exemple, vous devriez vous une nette différence lors des changements d'ordonnanceur (si ceux-ci sont fait correctement, du moins). Si un programme bloque, vous pouvez également utiliser cet outil pour mieux comprendre ce qui semble causer le blocage. Le script Python affiche sur la ligne de commande le dernier événement envoyé (autrement dit, la dernière chose que le programme a tenté de faire). Cela peut vous indiquer de quel côté est le problème.

> **Attention** : par défaut, les fonctions fournies n'enregistrent les données de profilage que toutes les 4 secondes (constante `PROFILAGE_INTERVALLE_SAUVEGARDE_SEC` dans `utils.h`) afin de limiter l'impact de l'écriture du fichier sur vos programmes; si votre programme bloque immédiatement au lancement, vous pouvez utiliser une valeur de `0` pour `PROFILAGE_INTERVALLE_SAUVEGARDE_SEC` afin de forcer l'enregistrement du fichier à _chaque nouvel événement_. Cela aura un impact significatif sur la performance des programmes, mais vous assurera que vous voyez bel et bien la dernière chose faite par chacun de vos programmes.

#### 7.2.3. Figures à remettre

Vous devrez remettre, avec votre code, les graphes produits par `creerProfilageImages.py` pour _tous les scénarios_ contenus dans le dossier `configs`. Vous devez faire fonctionner vos programmes _après initialisation_ pendant au moins 10 secondes, mais ne remettez pas un graphe couvrant plus de 20 secondes (utilisez au besoin l'argument `--duree` du script Python qui vous permet de tronquer les résultats). Nous pourrons vous poser des questions lors de l'évaluation sur les graphes que vous aurez remis.

#### 7.2.4. Désactivation du profilage

Le code servant à récupérer les informations de profilage ne respecte pas parfaitement les contraintes temps réel (accès à un fichier, réallocation mémoire potentielle, etc.). Bien que son impact pratique sur les performances soit très limité, vous pouvez le désactiver si vous voulez tester la performance maximale de votre code en allant dans le fichier `utils.h` pour y assigner la valeur 0 à `PROFILAGE_ACTIF`.

> C'est d'ailleurs ce qui est fait pour les solutions fournies. Les exécutables que nous vous fournissons ne produisent aucune information de profilage, pour éviter la confusion avec les informations produites par vos propres programmes.

<!-- #### ~~Optimisation par profilage~~

~~Dans les applications où la performance importe beaucoup, il est commun de **profiler** les programmes afin de permettre au compilateur de l'optimiser encore plus. Ce traitement s'effectue en deux passes et nécessite des programmes fonctionnels.~~

1. ~~Compilez votre code en mode _Release_, mais en ajoutant `-fprofile-generate` à la fin de la ligne 16 du fichier CMakeLists.txt (juste après `-flto`, mais avant le guillemet fermant). Exécutez-le par la suite sur le Raspberry Pi pendant une période de temps représentative. Cela générera des fichiers .gcda.~~
2. ~~Récupérez les fichiers .gcda depuis le Raspberry Pi et recompilez, cette fois en retirant `-fprofile-generate`, mais en ajoutant `-fprofile-use`. Prenez soin de fournir les fichiers .gcda dans le répertoire de compilation. GCC utilisera alors les informations de profilage pour optimiser encore plus le code produit.~~
 -->

## 8. Considérations pratiques

### 8.1. Dissipation thermique

Ce laboratoire impose une lourde charge au processeur du Raspberry Pi Zero. Afin de vous assurer de maximiser sa performance, assurez-vous qu'un dissipateur (_heatsink_) est installé sur celui-ci. Ne tentez **pas** de remplacer le dissipateur par d'autres objets métalliques tels que des pièces de monnaie, vous risquez de court-circuiter des composants importants du Raspberry Pi!

Vous pouvez connaître la vitesse d'horloge *courante* du processeur en utilisant la commande `sudo cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq`. Cette vitesse (en KHz) devrait être de 1 000 000 (1 GHz, la fréquence normale du processeur). Si elle est plus basse, vérifiez la température du processeur avec la commande `cat /sys/class/thermal/thermal_zone0/temp` (la valeur retournée doit être divisée par 1000 pour connaître la température en Celsius). Celle-ci ne devrait pas être supérieure à 65 degrés Celsius!

### 8.2. Mise en veille de la sortie HDMI

Par défaut, le Raspberry Pi Zero met en veille son GPU lorsqu'il n'y a pas d'interaction depuis un certain temps. Étant donné que vous accéderez au Raspberry Pi à distance, ce comportement est fâcheux. Si cela vous arrive, vous pouvez l'éviter en éditant (en mode sudo) le fichier /boot/cmdline.txt, et ajoutez, à la fin de la ligne, le texte `consoleblank=0`.

### 8.3. Nombre de FPS typique pour chaque configuration

Le tableau suivant récapitule le nombre d'images par seconde obtenu par notre solution ainsi que le délai maximum entre 2 images pour chaque source de chaque configuration. Pour chaque paire configuration/source, 2 nombres sont données, le premier étant le nombre d'images par seconde et le second le délai maximum en millisecondes. Notez que notre solution limite le délai entre 2 images à 34 ms minimum, soit environ 29 images par seconde. Notez finalement que ce tableau ne présente pas une information importante, soit la _variabilité_ de chaque mesure au fil du temps.

| Configuration  | Source 1  | Source 2  | Source 3  | Source 4  |
|---|---|---|---|---|
| 01  | 28.6 / 39.8  | N/A  | N/A  | N/A  |
| 02  | 24.7 / 60.2  |  24.8 / 61.5 | N/A  | N/A  |
| 03  | 20.2 / 117.8  | 20.3 / 131.2  | 20.5 / 110.0  | N/A  |
| 04  | 15.1 / 151.0  | 15.2 / 160.1  | 14.9 / 177.7  | 15.4 / 158.3  |
| 05  | 6.8 / 203.6  | N/A  | N/A  | N/A  |
| 06  | 1.5 / 732.8 | 1.5 / 729.0  | 1.5 / 747.6 | N/A  |
| 07  | 16.4 / 109.9  | 16.5 / 120.8 | N/A | N/A  |
| 08  | 12.5 / 158.2  | 12.6 / 158.8 | N/A | N/A  |
| 09  | 16.0 / 116.1  | 0.8 / 2318.9  | 0.7 / 1978.4  | 0.9 / 2124.6  |
| 10  | 13.4 / 127.2  |  10.7 / 151.1 | N/A  | N/A  |
| 11  | 19.9 / 79.8  | 6.8 / 256.1  | 16.9 / 166.5 | N/A  |

Vous ne serez pas évalués sur l'atteinte précise de ces performances. Toutefois, une performance drastiquement inférieure (moins de la moitié de la performance affichée ici, par exemple) pourra être pénalisée car elle implique une implémentation probablement incorrecte au niveau du partage mémoire et/ou de la synchronisation. De manière générale, votre solution devrait afficher de manière fluide (à l'oeil) les configurations `01_sourceUnique` et `02_deuxVideos`. Si votre implémentation est efficace, vous devriez être capable d'avoir aussi une fluidité acceptable pour `03_troisVideos` et, si elle est très efficace, une fluidité minimale (>= 15 fps) pour `04_mosaique`.

## 9. Modalités d'évaluation

Ce travail doit être réalisé **en équipe de deux**, la charge de travail étant à répartir équitablement entre les deux membres de l'équipe. Aucun rapport n'est à remettre, mais vous devez soumettre votre code source **et les graphes de profilage correspondant à tous les scénarios** dans monPortail avant le **27 février 2025, 17h00**. Ensuite, lors de la séance de laboratoire du **28 février 2025**, les deux équipiers doivent être en mesure individuellement d'expliquer leur approche, de comprendre les graphes de profilage qu'ils ont produits et de démontrer le bon fonctionnement de l'ensemble de la solution de l'équipe du laboratoire. Si vous ne pouvez pas vous y présenter, contactez l'équipe pédagogique du cours dans les plus brefs délais afin de convenir d'une date d'évaluation alternative. Ce travail compte pour **15%** de la note totale du cours. Comme pour les travaux précédents, votre code doit compiler **sans avertissements** de la part de GCC.

Notre évaluation se fera sur le Raspberry Pi de l'enseignant ou de l'assistant et comprendra notamment les éléments suivants:
  1. La sortie de compilation d'un `CMake: Clean Rebuild`;
  2. L'exécution des scripts 01, 02, 04, 08, 09 et 11 (fournis dans le dossier _configs_). Nous observerons également le contenu de _stats.txt_ entre chaque exécution.
  3. Dans le cas des configurations 09 et 11, nous validerons également que les changements d'ordonnanceur requis ont bien été effectués, par exemple en lançant la commande `htop` dans un autre terminal et en validant que les programmes ont une valeur de priorité conforme.
  4. Notez qu'il est possible que nous exécutions d'autres configurations : aucune des configurations fournies ne devrait faire planter vos programmes.
  5. Une discussion sur les résultats que vous obtenez pour ces diverses configurations, sur les graphes que vous avez remis et sur votre implémentation.
 
 
### 9.1. Barème d'évaluation

Le barême d'évaluation détaillé sera le suivant (laboratoire noté sur 20 points) :

#### 9.1.1. Qualité du code remis (6 points)

* (3 pts) Le code C est valide, complet et ne contient pas d'erreurs empêchant le bon déroulement des programmes.
* (1 pts) Tous les programmes compilent sans avertissement (*warning*) de la part du compilateur.
* (2 pts) Les programmes respectent les contraintes des programmes temps réel (pas d'allocation mémoire dynamique dans la section critique, pas d'entrée/sortie autrement que pour les fichiers de log, etc.)

#### 9.1.2. Validité de la solution (9 points)

> **Attention** : un programme ne compilant pas obtient automatiquement une note de **zéro** pour cette section.

* (2 pts) Les programmes sont en mesure d'utiliser des espaces mémoire partagés pour communiquer entre eux.
* (2 pts) La synchronisation entre les programmes est adéquate et fonctionnelle (c.-à-d. pas d'images coupées ou d'autres problèmes visuels).
* (3 pts) Le système complet est en mesure d'afficher une ou plusieurs vidéos sur l'écran de manière fluide et de chaîner les traitements effectués sur ces vidéos. Le nombre d'images par seconde est (le cas échéant) limité pour ne pas dépasser la valeur fournie dans le fichier vidéo.
* (2 pts) Les fonctions de l'ordonnanceur sont correctement utilisées pour sélectionner différents modes d'ordonnancement.

#### 9.1.3. Justesse des explications et réponses aux questions (5 points)

* (5 pts) Les étudiants sont en mesure d'expliquer l'approche utilisée et de répondre aux questions concernant leur code et la théorie liée au laboratoire. Les étudiants expliquent également correctement l'impact des différents modes d'ordonnancement et des différentes options d'exécution possibles. Les étudiants sont capables d'analyser le comportement de leurs programmes grâce aux figures de profilage et à expliquer leur contenu.




## 10. Ressources et lectures connexes

* Les [pages de manuel (man) de Linux](http://man7.org/linux/man-pages/index.html). Ces pages sont également disponibles sur la plupart des ordinateurs utilisant Linux, en tapant la commande `man nom_de_la_commande`.
* Une [référence en ligne](http://www.cplusplus.com/reference/clibrary/) de la librairie standard du langage C.
* Une [suite de billets de blog](https://raspberrycompote.blogspot.ca/2012/12/low-level-graphics-on-raspberry-pi-part_9509.html) extrêmement intéressante sur l'utilisation directe du framebuffer sur un Raspberry Pi.
* Un article en deux parties ([partie 1](https://lwn.net/Articles/743740/), [partie 2](https://lwn.net/Articles/743946/)) expliquant dans le détail le mode d'ordonnancement du scheduler DEADLINE.
