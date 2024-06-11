#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void create_record(int sock);
void read_records(int sock);
void update_record(int sock);
void delete_record(int sock);

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    int choice;

    // Creazione del socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Conversione dell'indirizzo IP
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    // Connessione al server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection failed\n");
        return -1;
    }

    while (1) {
        printf("\n\nScegli un'operazione:\n");
        printf("1. Aggiungi un nuovo ricambio\n");
        printf("2. Visualizza i ricambi\n");
        printf("3. Aggiorna un ricambio\n");
        printf("4. Elimina un ricambio\n");
        printf("5. Esci\n");
        printf("Scelta: ");
        scanf("%d", &choice);
        printf("\n\n");
        getchar(); // per consumare il newline lasciato da scanf

        switch (choice) {
            case 1:
                create_record(sock);
                break;
            case 2:
                read_records(sock);
                break;
            case 3:
                read_records(sock);
                update_record(sock);
                break;
            case 4:
                read_records(sock);
                delete_record(sock);
                break;
            case 5:
                close(sock);
                exit(0);
            default:
                printf("Scelta non valida.\n");
        }
    }

    return 0;
}

void create_record(int sock) {
    char name[BUFFER_SIZE], description[BUFFER_SIZE];
    int quantity;
    double price;
    char query[BUFFER_SIZE];

    printf("Inserisci il nome del ricambio altrimenti premi 8 per tornare indietro ed annulare l'operazione: ");
    fgets(name, BUFFER_SIZE, stdin);
    name[strcspn(name, "\n")] = '\0'; // rimuove il newline

    if (strcmp(name, "8") == 0) {
        return; // Torna indietro
    }

    printf("Inserisci la descrizione del ricambio: ");
    fgets(description, BUFFER_SIZE, stdin);
    description[strcspn(description, "\n")] = '\0'; // rimuove il newline

    printf("Inserisci la quantità: ");
    scanf("%d", &quantity);
    getchar(); // per consumare il newline lasciato da scanf

    printf("Inserisci il prezzo: ");
    scanf("%lf", &price);
    getchar(); // per consumare il newline lasciato da scanf

    snprintf(query, BUFFER_SIZE,
             "INSERT INTO Ricambi (Nome, Descrizione, Quantità, Prezzo) VALUES ('%s', '%s', %d, %.2f);",
             name, description, quantity, price);

    send(sock, query, strlen(query), 0);

    char response[BUFFER_SIZE] = {0};
    int valread = read(sock, response, BUFFER_SIZE);
    printf("%s\n", response);
}

void read_records(int sock) {
    char query[] = "SELECT * FROM Ricambi;";
    send(sock, query, strlen(query), 0);

    char response[BUFFER_SIZE] = {0};
    int valread;
    printf("________________________________________________________________\n\n");
    while ((valread = read(sock, response, BUFFER_SIZE - 1)) > 0) {
        response[valread] = '\0';
        if (strstr(response, "<END_OF_DATA>") != NULL) {
            response[valread - strlen("<END_OF_DATA>")] = '\0'; // Rimuove l'indicatore dal buffer
            printf("%s", response);
            break;
        }
        printf("%s", response);
    }
    printf("________________________________________________________________\n\n");
}

void update_record(int sock) {
    int id;
    char name[BUFFER_SIZE] = "";
    char description[BUFFER_SIZE] = "";
    int quantity = -1;
    double price = -1.0;
    char query[BUFFER_SIZE];
    char choice;

    while (1) {
        printf("\nInserisci l'ID del ricambio da aggiornare altrimenti premi 0 per tornare indietro e annullare l'operazione: ");
        if (scanf("%d", &id) != 1) {
            printf("Input non valido. Inserisci un numero valido.\n");
            while (getchar() != '\n'); // Consuma input residuo nel buffer
            continue;
        }
        getchar(); // per consumare il newline lasciato da scanf

        if (id == 0) {
            return; // Torna indietro
        }

        // Query per verificare se l'ID esiste nel database
        snprintf(query, BUFFER_SIZE, "SELECT * FROM Ricambi WHERE ID=%d;", id);
        send(sock, query, strlen(query), 0);

        char response[BUFFER_SIZE] = {0};
        int valread = read(sock, response, BUFFER_SIZE);
        if (strcmp(response, "") == 0) {
            printf("L'ID specificato non esiste. Inserisci un ID valido.\n");
            continue; // Richiede all'utente di inserire nuovamente l'ID
        } else {
            break; // Esce dal ciclo se l'ID è valido
        }
    }

    while (1) {
        printf("\n\nScegli il campo da aggiornare:\n");
        printf("1. Nome\n");
        printf("2. Descrizione\n");
        printf("3. Quantità\n");
        printf("4. Prezzo\n");
        printf("5. Fine\n");
        printf("Scelta: ");
        scanf("%c", &choice);
        getchar(); // per consumare il newline lasciato da scanf

        switch (choice) {
            case '1':
                printf("Inserisci il nuovo nome del ricambio: ");
                fgets(name, BUFFER_SIZE, stdin);
                name[strcspn(name, "\n")] = '\0'; // rimuove il newline
                break;
            case '2':
                printf("Inserisci la nuova descrizione del ricambio: ");
                fgets(description, BUFFER_SIZE, stdin);
                description[strcspn(description, "\n")] = '\0'; // rimuove il newline
                break;
            case '3':
                printf("Inserisci la nuova quantità: ");
                scanf("%d", &quantity);
                getchar(); // per consumare il newline lasciato da scanf
                break;
            case '4':
                printf("Inserisci il nuovo prezzo: ");
                scanf("%lf", &price);
                getchar(); // per consumare il newline lasciato da scanf
                break;
            case '5':
                goto end_update;
            default:
                printf("Scelta non valida. Riprova.\n");
        }
    }
    end_update:

    // Controlla se nessun campo è stato modificato
    if (strlen(name) == 0 && strlen(description) == 0 && quantity == -1 && price == -1.0) {
        printf("Nessun campo è stato modificato\n");
        return;
    }

    // Costruzione dinamica della query di aggiornamento
    snprintf(query, BUFFER_SIZE, "UPDATE Ricambi SET ");
    int first = 1;
    if (strlen(name) > 0) {
        snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Nome='%s'", name);
        first = 0;
    }
    if (strlen(description) > 0) {
        if (!first) {
            snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), ", ");
        }
        snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Descrizione='%s'", description);
        first = 0;
    }
    if (quantity != -1) {
        if (!first) {
            snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), ", ");
        }
        snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Quantità=%d", quantity);
        first = 0;
    }
    if (price != -1.0) {
        if (!first) {
            snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), ", ");
        }
        snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Prezzo=%.2f", price);
    }
    snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), " WHERE ID=%d;", id);

    send(sock, query, strlen(query), 0);

    char response[BUFFER_SIZE] = {0};
    int valread = read(sock, response, BUFFER_SIZE);
}

void delete_record(int sock) {
    int id;
    char query[BUFFER_SIZE];

    while (1) {
        printf("Inserisci l'ID del ricambio da eliminare altrimenti premi 0 per tornare indietro e annullare l'operazione: ");
        if (scanf("%d", &id) != 1) {
            printf("Input non valido. Inserisci un numero valido.\n");
            while (getchar() != '\n'); // Consuma input residuo nel buffer
            continue;
        }
        getchar(); // per consumare il newline lasciato da scanf

        if (id == 0) {
            return; // Torna indietro
        }

        // Query per verificare se l'ID esiste nel database
        snprintf(query, BUFFER_SIZE, "SELECT * FROM Ricambi WHERE ID=%d;", id);
        send(sock, query, strlen(query), 0);

        char response[BUFFER_SIZE] = {0};
        int valread = read(sock, response, BUFFER_SIZE);
        if (strcmp(response, "") == 0) {
            printf("L'ID specificato non esiste. Inserisci un ID valido.\n");
            continue; // Richiede all'utente di inserire nuovamente l'ID
        } else {
            break; // Esce dal ciclo se l'ID è valido
        }
    }

    // Costruzione della query di eliminazione
    snprintf(query, BUFFER_SIZE, "DELETE FROM Ricambi WHERE ID=%d;", id);

    // Invio della query al server
    send(sock, query, strlen(query), 0);

    // Lettura della risposta dal server (facoltativa, dipende dal tuo implementazione del server)
    char response[BUFFER_SIZE] = {0};
    int valread = read(sock, response, BUFFER_SIZE);
    printf("%s\n", response); // Stampa la risposta del server
}