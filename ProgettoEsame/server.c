#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sqlite3.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

void *handle_client(void *arg);
void execute_query(char *query, int client_sock);
void execute_purchase(int id, int quantity, int client_sock);

sqlite3 *db;
pthread_mutex_t db_mutex; // Mutex per la mutua esclusione sul database

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Inizializzazione del mutex
    pthread_mutex_init(&db_mutex, NULL);

    // Creazione del socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configurazione dell'indirizzo
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Binding del socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Ascolto delle connessioni
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Apertura del database
    if (sqlite3_open("magazzino.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("Server in ascolto sulla porta %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket) != 0) {
            perror("pthread_create failed");
            close(new_socket);
        }
    }

    sqlite3_close(db);
    pthread_mutex_destroy(&db_mutex); // Distruzione del mutex
    return 0;
}

void calculate_price(int id, int quantity, int client_sock);

void *handle_client(void *arg) {
    int sock = *(int *)arg;
    char buffer[BUFFER_SIZE] = {0};
    int valread;

    while ((valread = read(sock, buffer, BUFFER_SIZE)) > 0) {
        buffer[valread] = '\0';

        pthread_mutex_lock(&db_mutex); // Lock per la mutua esclusione sul database
        if (strncmp(buffer, "PURCHASE", 8) == 0) {
            int id, quantity;
            sscanf(buffer, "PURCHASE %d %d", &id, &quantity);
            execute_purchase(id, quantity, sock);
        } else if (strncmp(buffer, "PRICE", 5) == 0) {
            int id, quantity;
            sscanf(buffer, "PRICE %d %d", &id, &quantity);
            calculate_price(id, quantity, sock);
        } else {
            execute_query(buffer, sock);
        }
        pthread_mutex_unlock(&db_mutex); // Unlock per la mutua esclusione sul database
    }

    close(sock);
    pthread_exit(NULL);
}

void execute_purchase(int id, int quantity, int client_sock) {
    char query[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    sqlite3_stmt *res;
    double price;
    int current_quantity;

    // Step 1: Retrieve the current quantity and price
    snprintf(query, BUFFER_SIZE, "SELECT Quantità, Prezzo FROM Ricambi WHERE ID=%d;", id);
    if (sqlite3_prepare_v2(db, query, -1, &res, 0) != SQLITE_OK) {
        snprintf(response, BUFFER_SIZE, "Failed to execute statement: %s", sqlite3_errmsg(db));
        send(client_sock, response, strlen(response), 0);
        return;
    }

    if (sqlite3_step(res) == SQLITE_ROW) {
        current_quantity = sqlite3_column_int(res, 0);
        price = sqlite3_column_double(res, 1);
    } else {
        snprintf(response, BUFFER_SIZE, "Ricambio non trovato.");
        send(client_sock, response, strlen(response), 0);
        sqlite3_finalize(res);
        return;
    }
    sqlite3_finalize(res);

    // Step 2: Check if there are enough items in stock
    if (current_quantity < quantity) {
        snprintf(response, BUFFER_SIZE, "Quantità insufficiente in magazzino.");
        send(client_sock, response, strlen(response), 0);
        return;
    }

    // Step 3: Update the quantity
    snprintf(query, BUFFER_SIZE, "UPDATE Ricambi SET Quantità=Quantità-%d WHERE ID=%d;", quantity, id);
    if (sqlite3_exec(db, query, 0, 0, 0) != SQLITE_OK) {
        snprintf(response, BUFFER_SIZE, "Failed to update quantity: %s", sqlite3_errmsg(db));
        send(client_sock, response, strlen(response), 0);
        return;
    }

    // Step 4: Calculate and send the total price
    double total_price = price * quantity;
    snprintf(response, BUFFER_SIZE, "Acquisto completato. Il prezzo totale è: %.2f euro.", total_price);
    send(client_sock, response, strlen(response), 0);
}

void execute_query(char *query, int client_sock) {
    char *err_msg = 0;
    sqlite3_stmt *res;

    if (strstr(query, "SELECT") != NULL) {
        int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
        if (rc != SQLITE_OK) {
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "Failed to execute statement: %s", sqlite3_errmsg(db));
            send(client_sock, response, strlen(response), 0);
            return;
        }

        char response[BUFFER_SIZE];
        int cols = sqlite3_column_count(res);
        while (sqlite3_step(res) == SQLITE_ROW) {
            for (int col = 0; col < cols; col++) {
                const char *column_text = (const char *)sqlite3_column_text(res, col);
                if (strcmp(sqlite3_column_name(res, col), "Quantità") == 0) {
                    snprintf(response, BUFFER_SIZE, "%s = %s pz\n", 
                             sqlite3_column_name(res, col), column_text);
                } else if (strcmp(sqlite3_column_name(res, col), "Prezzo") == 0) {
                    snprintf(response, BUFFER_SIZE, "%s = %s €\n", 
                             sqlite3_column_name(res, col), column_text);
                } else {
                    snprintf(response, BUFFER_SIZE, "%s = %s\n", 
                             sqlite3_column_name(res, col), column_text);
                }
                send(client_sock, response, strlen(response), 0);
            }
            snprintf(response, BUFFER_SIZE, "\n");
            send(client_sock, response, strlen(response), 0);
        }

        snprintf(response, BUFFER_SIZE, "<END_OF_DATA>");
        send(client_sock, response, strlen(response), 0);

        sqlite3_finalize(res);
    } else {
        char response[BUFFER_SIZE];
        int rc = sqlite3_exec(db, query, 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            snprintf(response, BUFFER_SIZE, "SQL error: %s", err_msg);
            sqlite3_free(err_msg);
        } else {
            snprintf(response, BUFFER_SIZE, "\n\nModifica avvenuta con successo\n\n");
        }
        send(client_sock, response, strlen(response), 0);
    }
}

void calculate_price(int id, int quantity, int client_sock) {
    char query[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    sqlite3_stmt *res;
    double price;
    int current_quantity;

    // Step 1: Retrieve the current quantity and price
    snprintf(query, BUFFER_SIZE, "SELECT Quantità, Prezzo FROM Ricambi WHERE ID=%d;", id);
    if (sqlite3_prepare_v2(db, query, -1, &res, 0) != SQLITE_OK) {
        snprintf(response, BUFFER_SIZE, "Failed to execute statement: %s", sqlite3_errmsg(db));
        send(client_sock, response, strlen(response), 0);
        return;
    }

    if (sqlite3_step(res) == SQLITE_ROW) {
        current_quantity = sqlite3_column_int(res, 0);
        price = sqlite3_column_double(res, 1);
    } else {
        snprintf(response, BUFFER_SIZE, "Ricambio non trovato.");
        send(client_sock, response, strlen(response), 0);
        sqlite3_finalize(res);
        return;
    }
    sqlite3_finalize(res);

    // Step 2: Check if there are enough items in stock
    if (current_quantity < quantity) {
        snprintf(response, BUFFER_SIZE, "Quantità insufficiente in magazzino.");
        send(client_sock, response, strlen(response), 0);
        return;
    }

    // Step 3: Calculate the total price
    double total_price = price * quantity;
    snprintf(response, BUFFER_SIZE, "Il prezzo totale per %d pezzi è: %.2f euro.", quantity, total_price);
    send(client_sock, response, strlen(response), 0);
}
