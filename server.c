#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#define FYI 0x01
#define MYM 0x02
#define END 0x03
#define TXT 0x04
#define MOV 0x05


///////// Helper functions

int equal_sockaddr(struct sockaddr addr_one, struct sockaddr addr_two){
    /*
    *  Compares two sockaddr structs.
    *  Returns 1 if addr_one and addr_two correspond to the same adress, 0 otherwise.
    */

    int equal = 1;
    if (addr_one.sa_family != addr_two.sa_family){
        equal = 0;
    }
    else{
        int i;
        for (i=0; i<14; i++){
            if (addr_one.sa_data[i] != addr_two.sa_data[i]){
                equal = 0;
                break;
            }
        }
    }

    return equal;
}

///////// Server functions

int winner_board(int board[3][3]){
    /*
    *
    *  Takes game grid as input. If a player won, return its number. Returns 0 for draw, or -1 if nobody won.
    *  
    */
    
    // If winner on diagonal
    if (((board[0][0]==board[1][1] && board[1][1]==board[2][2]) || (board[0][2]==board[1][1] && board[1][1]==board[2][0])) && board[1][1] != 0){
        return board[1][1];
    }
    // If winner on rows/columns
    int i,j;
    for (i=0; i<3; i++){
        if (((board[0][i]==board[1][i] && board[1][i]==board[2][i]) || (board[i][0]==board[i][1] && board[i][1]==board[i][2])) && board[i][i]!=0){
            return board[i][i];
        }
    }
    // If draw
    for (i=0; i<3; i++){
        for (j=0; j<3; j++){
            if (board[i][j] == 0){
                // Not draw
                return -1;
            }
        }
    }
    // Draw
    return 0;
}

char* receive_msg(int sockfd, int* message_len, struct sockaddr* sender, socklen_t* adress_len){
    /*
    *   Reads one message from the socket described by "sockfd"
    *   Stores the message length, the sender adress and the adress length in the memory adresses pointed by respectively message_len, sender and adress_len.
    *   Returns a pointer to the place in the heap where the message is located
    */

    // First compute size of message    
    int msg_s = 5;
    int k = 1;
    // test buffer to probe message length
    char* test = malloc(msg_s);

    if (test == NULL){
            fprintf(stderr,"Error reading the message (not enough memory?)\n");
            *message_len=0;
            return NULL;
    }
    // while the message received is longer than the size of the buffer
    while (recvfrom(sockfd, test, msg_s*k, MSG_PEEK, NULL, NULL) == msg_s*k){
        ++k; // increase buffer's size
        test = realloc(test, msg_s*k);
        if (test == NULL){
            fprintf(stderr,"Error reading the message (not enough memory?)\n");
            *message_len=0;
            return NULL;
        }
    }
    free(test);
    // We now know what buffer size to allocate.

    // Final read of the message and store client data
    char* message_in = malloc(msg_s*k);

    *message_len = recvfrom(sockfd, message_in, msg_s*k, 0, sender, adress_len);
    if (*message_len == -1){
        fprintf(stderr,"Error finally reading the message, %s\n", strerror(errno)); // should not happen as we freed memory before
        *message_len=0;
        return NULL;
    }

    return message_in;
}


///////// Main game function

int game_loop(int arg_count, char* args[], int sockfd){
    // First step : listen to clients. As it is turn based, no need for threads.

    // Build data structures to hold clients
    struct sockaddr clients[2];
    socklen_t clients_addr_len[2] = {sizeof(clients[0]), sizeof(clients[1])};
    unsigned short nb_clients = 0;
    char* message_in_hello;
    int   message_in_hello_len;

    char msg_hello[7] = " Hello";//space before for the type byte
    msg_hello[0] = TXT;

    printf("Waiting for clients... \n");

    // Wait to have 2 clients
    while (nb_clients<2){
        // Receive the potential client's message.
        message_in_hello = receive_msg(sockfd, &message_in_hello_len, &(clients[nb_clients]), &(clients_addr_len[nb_clients]));
        if(message_in_hello==NULL){
            fprintf(stderr,"Error receiving message");
            return 2;
        }

        // Check that the recived message indeed corresponds to `msg_hello`
        int correct = 1;
        if (message_in_hello_len != 7){correct = 0;}
        else{
            int i;
            for (i=0; i<7; i++){
                if (message_in_hello[i] != msg_hello[i]){
                    correct = 0;
                    break;
                }
            }
        }

        // If the message is not good, don't register as a client
        if (correct != 1){
            free(message_in_hello);
            continue;
        }

        // Otherwise, "save" the new client
        if (nb_clients == 0){
            char greeting[] = " Welcome player 1, you will play with X";
            greeting[0] = TXT;
            sendto(sockfd, greeting, strlen(greeting), 0, &(clients[nb_clients]), clients_addr_len[nb_clients]);
        }
        else{ // because of while loop condition, can only be 1 player
            char greeting[] = " Welcome player 2, you will play with O";
            greeting[0] = TXT;
            sendto(sockfd, greeting, strlen(greeting), 0, &(clients[nb_clients]), clients_addr_len[nb_clients]);
        }

        free(message_in_hello);
        nb_clients++;

        printf("Player %i connected\n", nb_clients);
    }

    printf("The two players are connected.\n");

    // Initialize game variables
    int board[3][3] = {{0,0,0}, {0,0,0}, {0,0,0}};
    int filled = 0;
    int turn = 1;

    char make_move[1]   = {MYM};
    char game_full[2]   = {END, 255}; // 255 is -1 as char
    char wrong_turn[]   = " Not your turn!"; // len 15 + 1
    wrong_turn[0]       = TXT; 

    char*           client_answer;
    int             client_answer_len;
    struct sockaddr temp_addr;
    socklen_t       temp_addr_len = sizeof(temp_addr);
    
    
    // Game loop
    while (1){
        client_answer = (char*) 0;
        // Starts to inform the current player of the state of the game
        char board_info[30]; // size 1+1+27+1 : "FYI n 9*3 \0" (without spaces)
        board_info[0] = FYI;
        board_info[1] = filled;
        int i, j;
        int index = 2;
        for (i=0; i<3; i++){
            for (j=0; j<3; j++){
                if (board[i][j] != 0){
                    board_info[index]   = board[i][j];
                    board_info[index+1] = j;
                    board_info[index+2] = i;
                    index += 3;
                }
            }
        }
        // send FYI
        sendto(sockfd, board_info, 30, 0, &(clients[turn-1]), clients_addr_len[turn-1]);

        // Ask for move
        while (1){
            sendto(sockfd, make_move, 1, 0, &(clients[turn-1]), clients_addr_len[turn-1]);

            do {
                if(client_answer != (char*)0){free(client_answer);}
                client_answer = receive_msg(sockfd, &client_answer_len, &(temp_addr), &(temp_addr_len));

                if(client_answer==NULL){
                    fprintf(stderr,"Error receiving message");
                    return 2;
                }

                // If not from the good player
                if (equal_sockaddr(temp_addr, clients[turn-1]) == 0){
                    // If a client trying to join the game, send him a no
                    if (equal_sockaddr(temp_addr, clients[2-turn]) == 0){
                        printf("Client rejected\n");
                        sendto(sockfd, game_full, 2, 0, &temp_addr, temp_addr_len);
                    }
                    // If the other player sent a message during the wait
                    else{
                        printf("Other player tried to play\n");
                        sendto(sockfd, wrong_turn, 16, 0, &temp_addr, temp_addr_len);
                    }
                }
            } while (equal_sockaddr(temp_addr, clients[turn-1]) == 0);

            // Now we know the answer is from the correct player

            // If valid, leave the loop, else try again
            int col = (int)client_answer[1];
            int row = (int)client_answer[2];
            if (client_answer_len == 3 && client_answer[0] == MOV && (col == 0 || col == 1 || col == 2) && (row == 0 || row == 1 || row == 2) && board[row][col]==0){
                break;
            }else{ // the position played is invalid! We notify the client
                char invalid_position[] = " You played an invalid position.";
                invalid_position[0] = TXT;
                // send TXT notification
                sendto(sockfd, invalid_position, strlen(invalid_position), 0, &(clients[turn-1]), clients_addr_len[turn-1]);
                // it will loop to another MYM message
            }
        }
        // We update the board according to the move
        board[(int)client_answer[2]][(int)client_answer[1]] = turn;
        filled++;
        free(client_answer);

        // Check if win (send END 0 = draw, or END winner)
        int win = winner_board(board);
        if (win != -1){
            char winner_str[2] = {END, win};
            sendto(sockfd, winner_str, 2, 0, &(clients[0]), clients_addr_len[0]);
            sendto(sockfd, winner_str, 2, 0, &(clients[1]), clients_addr_len[1]);
            break; // Leave game loop
        }

        // If no winner, we repeat turn with the other player
        turn = 3-turn; // 1 becomes 2, 2 becomes 1
    }

    printf("Game ended\n");
    
    return 0;
}


int main(int arg_count, char* args[]){
    // Check correct amount of arguments
    if (arg_count != 2){
        fprintf(stderr,"Incorrect amount of arguments passed; should be ./server PORT\n");
        return 1;
    }
    
    // Build the socket where to listen
    short unsigned int port_to_listen;
    if (sscanf(args[1], "%hu", &(port_to_listen)) != 1){
        fprintf(stderr,"Error reading port number\n");
        return 1;
    }

    struct sockaddr_in self;
    self.sin_family = AF_INET;
    self.sin_port = htons(port_to_listen);
    self.sin_addr.s_addr   = INADDR_ANY;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd==-1){
        fprintf(stderr,"Error creating socket");
        return 2;
    }

    printf("Socket created.\n");

    if (bind(sockfd, (struct sockaddr *)&self, sizeof(struct sockaddr)) != 0){
        fprintf(stderr,"Could not bind the socket\n");
        return 2;
    }

    printf("Bind to port %i.\n", port_to_listen);

    // Flush the socket
    char temptest[2] = "oo";
    if (recv(sockfd, temptest, 1, MSG_PEEK | MSG_DONTWAIT) != 0 && temptest[0]!='o'){
        printf("Trying to clear socket buffer...\n");
        int tempint = 2;
        struct sockaddr tempsock;
        socklen_t tempaddr = sizeof(tempsock);
        if(receive_msg(sockfd, &tempint, &tempsock, &tempaddr)==NULL){//receive_msg to discard it
            fprintf(stderr,"Error receiving message");
            return 2;
        }
    }
    printf("Socket buffer cleared.\n");

    // Enter main server loop
    int game_id = 1;
    while (1){
        printf("Creating game %i\n", game_id);
        int main_error = game_loop(arg_count, args, sockfd);
        if (main_error != 0){
            fprintf(stderr,"Error when re-building a game\n");
            shutdown(sockfd, SHUT_WR);
            return main_error;
        }
        game_id++;
    }

    shutdown(sockfd, SHUT_WR);
}
