#if !defined(COMM_H)

#define COMM_H

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <ops.h>

/**
 * Funzione aggiuntiva per scrivere un file nella memoria secondaria
 */
int WriteFilefromByte(const char* name, char* text, long size, const char* dirname);

/**
 * Setta le stampe anche nelle API
 */
void set_prints();

/*
 * Apre una connesione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la richiesta di connessione,
 * la connessione da parte del client ripetur dopo 'msec' millisecondi e fino allo scadere del tempo assoluto 'abstime' specificato
 * come terzo argomento. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/*
 * Chiude la connessione AF_UNIX associata al socket file sockname. Ritorna 0 in caso di successo, -1 in caso di fallimento, 
 * errno viene settato opportunamente.
 */
int closeConnection(const char* sockname);

/*
 * Richiede di apertura o di creazione di un file. 
 * Se viene passato il flag O_CREATE ed il file già esiste memorizzato nel server, oppure il file non esiste ed il flag
 *  O_CREATE non è stato specificato, viene ritornato un errore. 
 * In caso di successo, il file viene sempre aperto in lettura e scrittura, se viene passato il flag O_LOCK il file viene
 * aperto e/o creato in modalità locked.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 * I flag possono essere 0 = NONE, 1 = O_CREATE, 2 = O_LOCK, 3 = O_CREATE && O_LOCK
 */
int openFile(const char* pathname, int flags);

/*
 * Legge tutto il contenuto del file server (se esiste) ritornando un puntatore ad un'area allocatata sullo heap nel parametro 'buf', 
 * mentre 'size' conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In caso di errore, 'buf' e 
 * 'size' non sono validi. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene viene settato opportunamente.
 */
int readFile(const char* pathname, void** buf, size_t* size);

/*
 * Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client.
 * Se il server ha meno di ‘N’ file disponibili, li invia tutti. Se N <= 0 la richiesta al server è quella di
 * leggere tutti i file memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di
 * successo (cioè ritorna il n. di file effettivamente letti), -1 in caso di fallimento, errno viene settato opportunamente.
 * Se dirname non e' specificato non salva i file
 */
int readNfiles(int N, const char* dirname);

/*
 * Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
 * terminata con successo, è stata openFile. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito dal 
 * server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere  scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int writeFile(const char* pathname, const char* dirname);

/*
 * Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
 * nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
 * dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/*
*In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la
*richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
*immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non
*viene resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è specificato. Ritorna 0 in
*caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
*/
int lockFile(const char* pathname);

/*
* Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se l’owner della lock è il processo che
* ha richiesto l’operazione, altrimenti l’operazione termina con errore. Ritorna 0 in caso di successo, -1 in caso di
* fallimento, errno viene settato opportunamente.
*/

int unlockFile(const char* pathname);

/*
 * Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int closeFile(const char* pathname);

/*
 * Rimuove il file cancellandolo dal file storage server.
 */
int removeFile(const char* pathname);


#endif
