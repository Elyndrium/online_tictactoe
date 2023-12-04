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


int sockfd = -1;

int main(int argc, char* argv[]){
    
    if (argc < 3) {
	fprintf(stderr, "Missing arguments! Please enter IP_ADDRESS and PORT_NUMBER\n");
	return 1;
    }
    int ret;
    char *adress = argv[1];
    struct in_addr serv_adress;

    ret = inet_pton(AF_INET, adress, &serv_adress); // converts directly char adress to in_addr
    
    if (ret!= 1){
        fprintf(stderr,"Invalid adress\n");
        return 1;
    }

    unsigned short port;
    ret = sscanf(argv[2], "%hu", &port);
    if (ret != 1){
        fprintf(stderr,"Invalid port number\n");
        return -1;
    }

    //  create socket 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd==-1){
    fprintf(stderr, "Unable to create socket");
	return 2;
    }

    // prepare server sockaddr_in
    struct sockaddr_in serv_adress_in;
    memset(&serv_adress_in, 0, sizeof(struct sockaddr_in)); // to set the zeros at the end
    serv_adress_in.sin_port = htons(port);
    serv_adress_in.sin_family = AF_INET;
    serv_adress_in.sin_addr = serv_adress;

    // Connect to server
    char msg[7] = " Hello";
    msg[0] = TXT;
    if(sendto(sockfd, msg, 7, 0, (struct sockaddr *)&serv_adress_in, sizeof(struct sockaddr_in))==-1){
        fprintf(stderr, "Unable to send message");
	    return 2;
    }

    // Run the game
    while(1){
        
        unsigned int len = 50;

        // Initialize buffer
        char *buf = malloc(len);
        if(buf == NULL){
            fprintf(stderr,"Error allocating memory for message buffer.\n");
            return 3;
        }

        // Receive the full message
        unsigned int rcv;
        while ((rcv=recv(sockfd, buf, len, MSG_PEEK)) == len){ // while the size of what we receive is greater than the bufer's size
            len+=50;
            char *tmp = realloc(buf, len);// increasing the size of the buffer untill we get the complete message
            if (tmp == NULL){
                fprintf(stderr,"Error reallocating memory for message buffer.\n");
                free(buf);
                return 3;
            }
            buf = tmp;
        }
        
        // Overwrite to be sure not to have written too much
        if(recv(sockfd, buf, rcv, 0) == 0){
            fprintf(stderr,"Error; recv should have blocked (received 0 bytes)\n");
            return 3;
        }
        
        if (buf[0] == TXT){
            // Print received message
            printf("Received message from server: \n%s\n",buf);
        }
        else if (buf[0] == FYI){ // print the game grid
            char board[3][3] = {{' ',' ',' '},{' ',' ',' '},{' ',' ',' '}};
            int n = buf[1];
            int i, j;
            char X_or_O;
            for (i=0; i<n; i++){
                if (buf[(3*i)+2] == 1) {X_or_O = 'X';}
                else {X_or_O = 'O';}
                board[(int)buf[(3*i)+4]][(int)buf[(3*i)+3]] = X_or_O;
            }
            // Now display the board
            for (i=0; i<3; i++){
                for (j=0; j<3; j++){
                    printf("%c", board[i][j]);
                    if (j<2){printf("|");}
                    else {printf("\n");}
                }
                if (i<2){printf("-+-+-\n");}
            }
        }
        else if (buf[0] == MYM){ // prompt the user for an input
            int col = -1;
            int row = -1;

            while ((col != 0 && col != 1 && col != 2) || (row != 0 && row != 1 && row != 2)){
                printf("Please enter your next move in this format \"<col> <row>\" (values ranging 0-2)\n");

                // Get user input
                int msg_s = 5;
                char *msg = malloc(msg_s);
                if (msg == NULL){
                    fprintf(stderr,"Error reading the message (not enough memory?)\n");
                    return 3;
                }
                memset(msg, '\0', 1);
                // realloc the buffer untill it is big enough to contain the message
                while (strlen(fgets(msg+strlen(msg), msg_s, stdin)) == msg_s-1 && msg[strlen(msg)-1] != '\n'){
                    msg = realloc(msg, strlen(msg)+msg_s);
                    if (msg == NULL){
                        fprintf(stderr,"Error reading the message (not enough memory?)\n");
                        return 3;
                    }
                }
                // Voluntarily accept whatever has correct numbers in correct positions 
                //we don't want to complain for extra characters like whitespaces after the position 
                col = (int)msg[0] - '0';
                row = (int)msg[2] - '0';
                free(msg);
            }
            printf("Input read; played col %i and row %i\n", col, row);
            // Modify it to what we send to the server
            char message_input[3] = {MOV, col, row};
            // Send the message to the server
            sendto(sockfd, message_input, 3, 0, (struct sockaddr *)&serv_adress_in, sizeof(struct sockaddr_in));

        }
        else if (buf[0] == END){
            if (buf[1] == -1){printf("Not enough space to join current game...\n");} // 255 is -1 as char
            else if (buf[1] == 0){printf("Draw!\n");}
            else{printf("Player %i won !\n", buf[1]);}
            break;
        }
        
    }

    printf("Closing the program.\n");

    // shuting down socket to indicate that we won't be sending any data anymore
    shutdown(sockfd, SHUT_WR);
    return 0;
}
