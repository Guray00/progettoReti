# progetto per Reti Informatiche

## Avviare l'applicativo per la prima volta
Per avviare l'applicativo e compilarlo è sufficiente eseguire il comando `make -B build`, oppure avviare il file `./compile_and_run.sh`. 
*NB:* il file run.sh si occupa unicamente dell'avvio delle finestre dell'applicazione, motivo per cui è prima necessario compilare il progetto.

## Chat
Ogni messaggio inviato in una chat comporta le seguenti spunte:
- [X]:  Messaggio non recapitato a causa di un errore
- [*]:  Messaggio inviato ma non ricevuto dal destinatario
- [\**]: Messaggio inviato e ricevuto dal destinatario

## Costanti
Tutte le costanti presenti sono indicate nel file `./utils/costanti.h`

## Utenti
Di default sono presenti i seguenti utenti:

| Username  | Password   |
|---|---|
| user1  | user1  |
| user2  | user2  |
| user3  | user3  |

## Problemi noti
Può capitare che, al seguito del cambio di utente sulle varie shell, un device non sia in grado di contattarne un'altro restituendo una `connection refused`. Questo è probabilmente dovuto al fatto che i socket, una volta chiusi, non vengono liberati immediatamente.