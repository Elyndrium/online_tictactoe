# online-tictactoe
## How to run it?
You can make both the server and client program using the command `make` in a POSIX terminal.  
You can create a server with the command `./server <port_number>`.  
You can create a client with the command `./client <serv_ipaddr> <serv_port>`
## Message types (first byte of message) :

FYI 0x01  - send the board information to clients

MYM 0x02  - asks client for a move

END 0x03  - end the game or tells client that game is full

TXT 0x04  - send text message to clients to display

MOV 0x05  - client message to notify a move

LFT 0x06  - ???


## The client :

`./client <serv_ipaddr> <serv_port>`  
Sends "TXT Hello" to the server to connect

### On TXT receive

Display the content of the message

### On FYI receive

Store and display the board's current state

### On MYM receive

Waits for user input to answer "MOV \<col\> \<row\>" to the server for moves

### On END receive
  
Display the winner and exit

## The server :
`./server <port_number>`  
Listens on a given port for new clients, send back a greeting or "END <0xFF>" = "END <255>" if game is already full
  
Player 1 is the first to have connected (and player 2 the second)
  
### 1. FYI
  
The server informs the player of the state of the board
  
### 2. MYM
 
The server requests a move. If the move is not valid, notify through a TXT message and request a new move.
  
Then, update the board
  
### 3. Check if END
  
Check for winning positions or draws and send END messages with winner (or "0" if draw)
 
