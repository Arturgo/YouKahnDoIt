Ce programme implémente un réseau de Kahn séquentiel, exécuté en parallèle sur plusieurs processeurs de plusieurs machines.
Dans ce setup, nous avons implémenté une multiplication de matrices par diviser pour régner ainsi que la génération de nombres premiers.
L'architecture du réseau est constitué d'un serveur et de plusieurs clients.

Pour créer une instance de serveur, il faut exécuter la commande :
PORT=8000 ./main serv
Pour exéctuer une instance de client, il faut exécuter la commande :
PORT=8000 ./main

(Par défaut, tout est exécuté en local, même si l'on peut l'exécuter sur plusieurs machines)

Notre scheduleur est très basique et est aléatoire, une tâche est échangée entre le serveur et un client avec une certaine probabilité.
Chaque tâche a son propre modèle de mémoire, stocké comme une map de listes doublement chainée d'octets.
Lorsqu'une tâche est changée d'ordinateur, on envoie un pointeur de fonction qui correspond à la continuation de la tâche, suivie de sa mémoire, et ses canaux d'entrée.
Chaque canal est un buffer stocké par l'ordinateur qui exécute sa tâche de destination. Lorsqu'une tâche veut écrire dans un canal, elle le fait si le canal est possédé par cet ordinateur, sinon il envoie les octets au serveur, qui s'occupe de les renvoyer au bon client.
On utilise massivement des mutex pour éviter que les différents threads écrivent la même partie de la mémoire en même temps.
La synchronisation des canaux est effectuée en stockant pour chaque canal d'entrée/sortie le nombre d'octet qui ont été lus/écrits.
On garde l'ensemble des canaux vides pour éviter de scheduler des tâches qui ne vont rien pouvoir lire.

La sortie sortie standard est malheureusement encore répartie sur les différents processus.
Les performances sont évidemment moins bonnes qu'une multiplication de matrices "normales" : le coût du réseau et des mutexs est très lourd et nous n'avons pas cherché à l'optimiser, nous avons eu suffisament de mal à gérer la correction pour la synchronisation des canaux et des tâches.
