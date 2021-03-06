//header file needed for networking.c

#ifndef networking
#define networking

#define UNAMELENGTH 30 //max username length
#define MESSAGELENGTH 255 //max message lengh in chars
#define MAXTRANSMISSIONERRORS 10


//definitions and explanation are below
#define SB_BROADCAST 0
#define SB_QUIT 1
#define SB_KEYBOARDUPDATE 2
#define SB_GAUDY 3
#define SB_DSCANDIDATE 4
#define SB_DSVOTE 5
#define SB_DSEJECT 6
#define SB_TIMEOUT 7
#define SB_VOTESFORME 8
#define SB_LURK 9
#define SB_YELL 10

long sentBytes;
long recvBytes;
int errors;
char ** PDM;

typedef struct
{
    char userName[UNAMELENGTH];
    int majorVersion;
    int minorVersion;
    //maybe add sAddr so we can read that for the address of server
}ConnectInit;

typedef struct
{
    int status; //see notes below
    int id; //this is the id the server will give to client
}ConnectACK;

typedef struct
{
    int id; //if not set to 0 then means it's a keyboard update (10 people)
    int status; //used for lark, yelp, gauntlet - see project notes
    int messageLen; //message length for reading
    char message[UNAMELENGTH+MESSAGELENGTH]; //message is as long as messagelength, but when resending server
                                               //will attach username 
}Chat;

int connectToServer(char *, int);
int isConnected();
int closeServer();
int sendConnectInit(ConnectInit *);
int sendConnectInit(ConnectInit * cI);
int getACK(ConnectACK * cI);
int sendChat(Chat * ch);
int receiveChat(Chat * ch);
int serializeChat(char * msg, Chat * ch);
int deserializeChat(char * msg, Chat * ch);
int getSock();
int receivePDM();


//Struct NOTES - PLEASE READ!
//I cannot send the structs over as chars, due to bin packing and possible client server architecture difference 
//send as a char is almost suicice. Thought now all the problems apply to use I would be much happier
//doing a plain text format. So with that in mind I have adopeted this format for each of the chars
//
//ConnectInit = "majorVersion`minorVersion`username"
//ConnectAck = "status`id"
//Chat = "id`status`messageLength`message" (make sure message is \0 terminated, using netwking.c will ensure this)
//
//I have provided for the clients a networking.h/c for desearializing the strings that come in, they should
//return strucst, piece of cake. FYI all send and recv do a tiny handshake that solve 2 problems, first is
//sending message of how long the serialized data is, the second is the confirm the client is there.


//ConnectACK notes
//status bits: 0 - no erros, connection working
//             1 - too many conencted users, please try again
//             2 - username in use, change and reconnect
//             3 - something wrong with server - failed when getting user ID
//             4 - username cannot be NULL


//Chat notes
//status bits: 0 - nothing unusual, global broadcast
//status bits: 1 - user disconnecting...
//status bits: 2 - this is a keyboard update
//status bits: 3 - gaudy
//status bits: 4 - deep-six GET not voted for users list
//status bits: 5 - deep-six vote for user
//status bits: 6 - deep-size YOU GOT EJECTED
//status buts: 7 - idle timeout
//status bits: 8 - deep-six how mayne votes I have against me  
//status bits: 9 - lurk mode
//status bits: 10- yell mode
#endif
