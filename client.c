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
#define PORTA 9040 // porta su cui ascolta il server
#define INDIRIZZO_IP "127.0.0.1" // indirizzo su cui ascolta il server

/** 
 *  ---- DESCRIZIONE ----
 *  Applicazione che si occupa di effetturare una connessione TCP con un server e chiedere che
 *  vengano calcolate delle operazioni tra numeri reali per poi visualizzarne il risultato 
 */

/**
 * Viene letta la stringa dell'operazione da tastiera e viene formattata secondo le specifiche
 * @return il vettore che si aspetta di ricevere il server
 */
double* leggiOperazione();
/**
 * Viene stabilita la connessione TCP\IP tra client e server
 * @return socket sottoforma di int 
 */
int connessioneServer();

/**
 * @brief Ricevuta la risposta dal server, effettua la de-formattazione e restituisce la stringa con il risultato dell'operazione
 *        e il tempo di servizio
 * 
 * @return stringa da mostrare a schermo
 */
char* parsingRisposta(double []);

int main(int argc, char const *argv[])
{
    // Socket con la quale viene stabilita la connessione e su cui scrivere e leggere i messaggi
    int clientSocket = 0;
    // Prepara e stabilisce la connessione verso il server 
    clientSocket = connessioneServer();

    // Se la connessione è andata a buon fine ed è stato restiuito un socket corretto...
    if(clientSocket != -1){
        // Conterrà l'operazione letta da tastiera già formattata
        double* operazioneFormattata = NULL;
        // Conterrà il risultato e i due timestamp ricevuti dal server
        double risultatoFormattato[3];
        // Conterrà il risultato e il tempo di servizio sottoforma di stringa
        char *risultato = NULL;
        size_t dimensioneFormato = sizeof(double) * 3;

        // In attesa di ricevere la conferma dal server...
        int conferma = -1, n;
        printf("In attesa di poter effettuare operazioni...\n");
        // Se il server non si interrompe prima di inviare la conferma...
        if (read(clientSocket, &conferma, sizeof(int)) != 0) {
            // Se il server ha inviato effetivamente la conferma aspettata...
            if (conferma == 0) {
                // Leggi la stringa da tastiera e formattala
                // Finchè l'utente decide di scrivere qualcosa e di non terminare la comunicazione...
                while ((operazioneFormattata = leggiOperazione()) != NULL){
                    // Controllo se l'operazione è stata letta correttamente (se non lo è stato allora è semplicemente vuota)...
                    if(*operazioneFormattata == 0){
                        printf("E' stato inserito un formato di operazione non corretto, riprova \n\n");
                    } else{
                        // Invio l'operazione sul socket sottoforma di vettore di double
                        // Se non sono stati spediti correttamente il numero di byte necessario allora si è verificato un problema
                        if(write(clientSocket, operazioneFormattata, dimensioneFormato) != dimensioneFormato) {
                            printf("Errore durante la scrittura dell'operazione sul socket\n");    
                            // Non è più possibile inviare operazioni
                            break;
                        } 
                        // Viene letto il risultato scritto dal server sul socket
                        // Se non vengono letti i byte che ci si aspetta allora si è verificato un problema
                        if((n = read(clientSocket, risultatoFormattato, dimensioneFormato)) == dimensioneFormato) {
                            // Viene parsato il risultato e salvato sottoforma di stringa per poi essere stampata
                            risultato = parsingRisposta(risultatoFormattato);
                            printf("%s\n", risultato);
                        } else {
                            printf("Il server si è interrotto inaspettatamente\n");
                            // Non è più possibile inviare operazioni    
                            break;
                        }
                        // Libero lo spazio di memoria precedentemente allocato nell'heap
                        free(operazioneFormattata);
                        free(risultato); 
                    }                     
                }  
            }
        } else {
            printf("Il server si è interrotto inaspettatamente\n");
        }

        // Alla fine della comunicazione viene chiuso il socket
        if(close(clientSocket) != 0)
            printf("Errore durante la chiusura del socket\n");
        else 
            printf("Chiusa la comunicazione\n");
    }
        
    return 0;
}

double* leggiOperazione(){
    // ---- INTERFACCIA UTENTE ----
    printf("----- INVIO OPERAZIONE ----\n");
    printf("Digita l'operazione che desideri effettuare nel formato <Operando> [+-*\\] <Operando> \n");
    printf("Digita \"stop\" per uscire\nMessaggio: ");
    // ---- INIZIALIZZAZIONE ----
    // Variabili utilizzate solo per salvare l'input    
    double primoOperando, secondoOperando;
    char operatore;
    // Alloco la memoria per il formato dell'operazione e la setto a 0
    double *operazioneFormattata = (double *) malloc(3 * sizeof(double));
    memset(operazioneFormattata, 0, 3 * sizeof(double));

    // Variabile per salvare l'eventuale segnale di stop alla comunicazione voluto dall'utente
    char* uscita = (char *) malloc(5);
    // ---- LOGICA ----
    // Se vengono letti correttamente i 3 parametri interessati e quindi non ci sono problemi durante l'input...
    if (scanf("%lf %c %lf", &primoOperando, &operatore, &secondoOperando) == 3){
        
       // Se sono stati forniti esattamente 3 parametri e non è
       if(getchar() == '\n'){
           if ((operatore == '+' || operatore == '-' || operatore == '*' || operatore == '/') ){
                // Formattazione dell'operazione nel formato [operatore, operando, operando]
                operazioneFormattata[0] = operatore; // L'operatore viene salvato come double ma dopo castato a char dal server
                operazioneFormattata[1] = primoOperando; 
                operazioneFormattata[2] = secondoOperando;
           }
       } else 
            // Se c'è ancora qualche carattere nel buffer di input rimuovilo...
            while(getchar() != '\n');

         /*  
            Se l'operazione non è una di quelle consentite oppure c'è qualche altro carattere 
            nel formato dell'operazione, restituisci la struttura vuota
        */
        return operazioneFormattata;
    
    } else {
        // Prendo da input la stringa rimasta nel buffer
        scanf("%s", uscita);
        // Se l'utente vuole uscire...
        if (!strcmp(uscita, "stop")){
            printf("Hai deciso di terminare la comunicazione\n");
            free(uscita);
            return NULL;
        }
        // Svuoto i caratterri rimanenti sullo standard input
        while(getchar() != '\n');
    }
        
    return operazioneFormattata;
}

int connessioneServer(){
    int clientSocket;
    
    // ---- LOGICA DI CONTROLLO ----
    // Se la creazione del socket avviene con successo...
    if((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) != -1){
        struct sockaddr_in serverDescriptor;
        
        // Specifico che la famiglia degli indirizzi è IPv4
        serverDescriptor.sin_family = AF_INET;
        /* Salva l'indirizzio IP del server all'interno del campo s_addr della struttura sin_addr 
           che si trova nella struttura sockaddr_in */
        serverDescriptor.sin_addr.s_addr = inet_addr(INDIRIZZO_IP);
        // Il server ascolta su PORTA
        serverDescriptor.sin_port = htons(PORTA);

        printf("Connessione al server...\n"); 
        // Se la connessione al socket con l'indirizzo specificato nella struttura avviene con successo...
        if( connect(clientSocket, (struct sockaddr *)&serverDescriptor, sizeof(serverDescriptor)) != -1){
            printf("Connesso!\n");
            // Puoi restituire il socket e continuare il programma
            return clientSocket;
        } else
            printf("Errore durante la connessione al server\n");

    } else 
        printf("Errore durante la creazione del socket\n");

    // C'è stato un errore nella logica di controllo
    return -1;
}

char *parsingRisposta(double rispostaFormattata[]){
 
    // Alloco lo spazio necessario per concatenare le due stringhe di interfaccia insieme a i due valori calcolati
    char* risposta = (char *) malloc(80);

    // Trasformo in una stringa il risultato dell'operazione calcolata, calcolo il tempo di servizio e lo aggiungo alla stringa
    sprintf(risposta, "Risultato operazione = %.6g, tempo di servizio: %lf secondi\n", rispostaFormattata[2], rispostaFormattata[1] - rispostaFormattata[0]);

    // Restituisco la stringa della rispsota
    return risposta;

}