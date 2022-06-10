#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#define MAX_CLIENTS 10
#define PORTA 9040

// Puntatore al file di log su cui scrivere le varie operazioni effettuate
FILE* logFile = NULL;
// Semaforo che gestisce l'accesso alla sezione critica per la scrittura concorrente sul file di log
pthread_mutex_t sem;
// Semaforo che gestisce l'accesso alla sezione critica per modificare la condition
pthread_mutex_t semCond;
// Condition per gestire la disconessione di un client e l'avvio di un nuovo trhead
pthread_cond_t condition;
/**
 * @brief Dato il messaggio ricevuto, viene effettuata l'operazione specificata con i due operandi forniti e restituito il messaggio per il client
 * 
 * @param operazione vettore che contiene gli operandi e l'operazione da svolgere
 * @return vettore contenente il risultato dell'operazione e i due timestamp
 */
double* calcolaRisultato(double operazione[]);

/**
 * @brief Inizializza le strutture dati necessarie per l'apertura del socket e viene restiuito un socket in caso di successo, -1 altrimenti
 * 
 * @param numberOfClients numero massimo di client da mettere in coda
 
 * @return socket se creazione andata a buon fine, -1 altrimenti
 */
int initSocket(int numberOfClients);

/*
    Struttura dati che descrive le informazioni che riguardano il socket di un client  
*/
/* TODO: 
    - i thread mandano un messaggio se il client può inviare l'operazione
    - gestire la chiusura inaspettata del server con l'invio di un operazione non valida
*/
typedef struct {
    int socket;
    struct sockaddr_in addr;
    int index;
} clientDescriptor;

// Vettore che si occupa di gestire le informazioni dei thread quando si connettono e quando si disconnettono
clientDescriptor *clientsInfo[MAX_CLIENTS];
// Viene decrementata di un'unità ad ogni client che si connette
int clients = MAX_CLIENTS;
/**
 * @brief Gestisce la comunicazione con il client, il calcolo dell'operazione, la scrittura sul file di log delle operazioni richieste
 *  
 * @param clientInfo contiene l'indirizzo del client e il socket su cui leggere e scrivere
 */
void comunicazione(clientDescriptor *clientInfo) {
    // Conterrà il risultato dell'operazione calcolata e i due timestamp
    double *risultatoFormattato = NULL;
    // Conterrà la richiesta effettuata dal client
    double operazioneFormattata[3];
    // Conterrà i due timestamp che è necessario salvarsi
    struct timespec arrivoRichiesta, invioRisposta;

    // Finchè ci sono operazioni sul socket da leggere (si blocca quando il client sta facendo la write)
    while( read(clientInfo -> socket, operazioneFormattata, sizeof(double) * 3) == sizeof(double) * 3){
        // Salvo il timestamp dell'arrivo della richiesta
        clock_gettime(CLOCK_REALTIME, &arrivoRichiesta);
        
        printf("Mi hai chiesto di fare: %.6g %c %.6g\n", operazioneFormattata[1], (int)operazioneFormattata[0], operazioneFormattata[2]);
        fflush(stdout);

        // Viene parsata l'operazione da calcolare e formattato il risultato
        risultatoFormattato = calcolaRisultato(operazioneFormattata);
        // Nella risposta viene inserito il timestamp di arrivo della richiesta del client
        risultatoFormattato[0] = arrivoRichiesta.tv_sec + arrivoRichiesta.tv_nsec / 1000000000.0;

        // Restituisce una struttura dati che fornisce il giorno, l'anno, l'ora del timestamp in un formato human readable
        struct tm *my_tm = localtime(&arrivoRichiesta.tv_sec);
        char outputLog[200];
        /* Compongo la stringa  da stampare sul file di log_
           - Client: <Indirizzo IP>, Operazione: <Operando> <op> <Operando>, risultato: <ris>, arrivo richiesta: <date>
        */
        sprintf(outputLog, "Client: %s, Operazione: %.6g %c %.6g, risultato: %.6g, arrivo richiesta: %s", inet_ntoa(clientInfo -> addr.sin_addr), \
                operazioneFormattata[1], (int)operazioneFormattata[0], operazioneFormattata[2], risultatoFormattato[2], asctime(my_tm));

        // Se il lock sul semaforo avviene correttamente e si entra nella zona critica per scrivere sul log...
        if (pthread_mutex_lock(&sem) == 0){
            // Se la fputs restituisce EOF allora ci sono stati errori in scrittura...
            if (fputs(outputLog, logFile) == EOF){    
                printf("Errore durante la scrittura su file");
                // "Forza" la scrittura del buffer sullo standard output
                fflush(stdout);
            } else 
                // "Forza" la scrittura del buffer sul file di log
                fflush(logFile);

            // Se l'uscita dalla zona critica non avviene con successo vi è un errore...
            if (pthread_mutex_unlock(&sem) != 0) {
                printf("Errore nel unlock del mutex");
                // Forza la scrittura del buffer sullo standard output
                fflush(stdout);
            }
        } else 
            printf("Errore nel lock del mutex");
        
        printf("Ti mando il risultato...\n");
        fflush(stdout);

        // Salvo il timestamp attuale
        clock_gettime(CLOCK_REALTIME, &invioRisposta);
        // Nella risposta viene inserito il timestamp di invio della risposta al client
        risultatoFormattato[1] = invioRisposta.tv_sec + invioRisposta.tv_nsec / 1000000000.0;
        // Scrivo sul socket la risposta
        write(clientInfo -> socket, risultatoFormattato, sizeof(double) * 3);
    }
    
    // Chiude il socket di comunicazione con il client
    if (close(clientInfo -> socket) == 0){
        printf("Comunicazione con il client chiusa\n");
        fflush(stdout); 

        // Libero lo spazio dalle informazioni del client che si è disconnetto
        free(clientsInfo[clientInfo -> index]);
        clientsInfo[clientInfo -> index] = NULL;

        // Un client è uscito
        clients++;

        // Inizia la zona critica per permettere l'accesso alla pool dei thread ad un nuovo client
        pthread_mutex_lock(&semCond);
        // Un nuovo client può inviare oeprazioni
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&semCond);
    }
}

int main(int argc, char const *argv[])
{
    // Socket che rimanrrà in ascolto per accettare connessioni
    int serverSocket = -1;
    // Inizializzo il socket con numero di client nella coda pari a MAX_CLIENTS
    serverSocket = initSocket(MAX_CLIENTS);

    // Se il socket è stato creato con successo...
    if (serverSocket != -1)
    {
        // Vettore per salvere l'id di ogni thread
        pthread_t tid[10];
        // Vettore per salvare la struttura delle informazioni di ogni client connesso
        int sockApp;
        struct sockaddr_in addrApp;
        // Indice per gestire i due vettori
        int clilen = sizeof(struct sockaddr_in);

        // Messaggio da inviare per iniziare la comunicazione
        int conferma = 0;        
        // Se l'apertura del file di log è avvenuta correttamente...
        if ((logFile = fopen("operazioni.log", "a")) != NULL)
        {
            // Se l'inizializzazione del mutex è avvenuta con successo...
            if (pthread_mutex_init(&sem, NULL)  == 0) 
            {
                // Se l'inizializzazione della condition è avvenuta con successo...
                if(pthread_cond_init(&condition, NULL) == 0) 
                {
                    // Il server è in continuo ascolto di nuove connessione da accettare
                    while((sockApp = accept(serverSocket, (struct sockaddr *)&addrApp, (socklen_t *)&clilen)) != 0) 
                    {   
                        // Se ho raggiunto il numero massimo di client attivi
                        if(clients == 0) {
                            // Attendo la terminazione di un qualsiasi thread attraverso l'uso di una condition
                            pthread_mutex_lock(&semCond);
                            pthread_cond_wait(&condition, &semCond);
                            pthread_mutex_unlock(&semCond);
                        } 

                        // Controllo dov'è disponibile una locazione da allocare per salvare le informazioni del socket
                        for(int c = 0; c < MAX_CLIENTS; c++){
                            if(clientsInfo[c] == NULL){
                                // Alloco lo spazio per salvare le informazioni
                                clientsInfo[c] = malloc(sizeof(clientDescriptor));
                                // Salvo le informazioni necessarie
                                clientsInfo[c] -> index = c;
                                clientsInfo[c] -> socket = sockApp;
                                clientsInfo[c] -> addr = addrApp;
                                // Un nuovo client è attivo
                                clients--;
                  
                                // Comunico al client che puoi iniziare ad inviare le sue operazioni
                                write(sockApp, &conferma, sizeof(int));
                                // Creo un nuovo thread che andrà ad eseguire la funzione che si occupa di gestire lo scambio di 
                                // messaggi tra client e server                                
                                pthread_create(&tid[c], 0, (void *)comunicazione, clientsInfo[c]);
                                break;
                            }
                        }                            
                    }
                } 
            } else 
                printf("Errore nell'inizializzazione del mutex");
        } else 
            printf("Errore nell'apertura del file di log");
    } 
    return 0;
}

double* calcolaRisultato(double operazione[]){

    double risultatoOperazione;
    // Conterrà il risultato dell'operazione formattato sotto forma di vettore
    double* risultatoFormattato = NULL;
    
    // In base al carattere di operazione ricevuto (i char vengono visti come degli interi grazie al cast) calcolo il risultato
    switch((int)operazione[0]){
        case '+':   risultatoOperazione = operazione[1] + operazione[2];
                    break;
        case '-':   risultatoOperazione = operazione[1] - operazione[2];
                    break;
        case '*':   risultatoOperazione = operazione[1] * operazione[2];
                    break;
        case '/':   risultatoOperazione = operazione[1] / operazione[2];
                    break;
    }

    // Alloca abbastanza spazio per contenere i timestamp e il risultato dell'operazione
    risultatoFormattato = (double *) malloc(sizeof(double) * 3);
    risultatoFormattato[2] = risultatoOperazione;

    // Restituisce il vettore
    return risultatoFormattato;
}

int initSocket(int numberOfClients){
    int serverSocket = -1;
    struct sockaddr_in serverDescriptor;

    // Inizializzo un socket TCP che utilizza IPv4
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    // Se la creazione del socket è andata a buon fine
    if(serverSocket != -1){
        // Inizializzo la struttura che descrive l'indirizzo del socket che ascolta le comunicazioni
        // La famiglia degli indirizzi è IPv4
        serverDescriptor.sin_family = AF_INET;
        // La porta sulla quale avviene l'ascolto è la 9040
        serverDescriptor.sin_port = htons(PORTA);
        // L'ascolto avviene su tutte l'interfacce locali del server
        serverDescriptor.sin_addr.s_addr = inet_addr("0.0.0.0"); // INADDR_ANY è la macro

        // Se l'associazione del socket allo spacename dell'indirizzo avviene corretta mente
        if(bind(serverSocket, (struct sockaddr*)&serverDescriptor, sizeof(serverDescriptor)) != -1)
        {   
            // Se la preparazione per accettare le connessioni di numberOfClients avviene con successo, restituisci il socket creato
            if (listen(serverSocket, MAX_CLIENTS) != -1)
                return serverSocket;
        }
        else 
            printf("Errore durante l'assegnazione dell'indirizzo al socket\n");
    } else 
        printf("Errore durante la creazione del socket\n");

    // Si è verificato un qualsiasi tipo di errore
    return -1;
}