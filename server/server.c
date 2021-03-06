#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#include "../client/networking/networking.h"
#include "linkedlist.h"
#include "yell.h"

//prototypes
void* thread_proc(void *arg);
int setUserName(char * name);
int checkDupUserName(char * name);
void addMessage(char * msg);
int checkRecipients();
int getList(char *msg, int id);
int removeVotes(int id);

int maxUsers;
int numUsers;
int totalUsers;
int timeout;
int messageCount;
char **userNames;
char **dsVotes;
char **PDM;
int * dsVotesForMe;//hold the list of votes
int * userStatus;
int * messageStatus; //0-have not recieved message, 1-recieved message, -1 dropped user, 2 ejected
int getMessage(int id, char * msg);

sem_t lusers;
sem_t lmessage; 

int main(int argc, char *argv[])
{

    if(argc != 4)
    {
        printf("Must be in format: server port Max_Clients MaxIdle_Minutes\n");
        exit(EXIT_SUCCESS);
    }


    //get info from CL
    int port = atoi(argv[1]);
    maxUsers = atoi(argv[2]);
    timeout = atoi(argv[3])*60; //get seconds

    if(port == 0 || maxUsers == 0)
    {
        perror("Port or MaxUsers must be greater than 0");
    }

    //init 
    sem_init(&lusers,0,1);
    sem_init(&lmessage,0,1);
    numUsers = 0;
    totalUsers = 0;
    userStatus = malloc(maxUsers * sizeof(int));
    dsVotesForMe = malloc(maxUsers * sizeof(int));


    sem_init(&lmessage,0,1);

    //init of array of strings
    userNames = malloc(maxUsers * sizeof(char *));
    int i = 0;
    for(i=0; i< maxUsers; i++)
    {
        userNames[i] = malloc(UNAMELENGTH * sizeof(char));
        strcpy(userNames[i],"\0");
    }

    dsVotes = malloc(maxUsers * sizeof(char *));
    for(i=0; i<maxUsers; i++)
    {
        dsVotes[i] = malloc(50 * sizeof(char));
        strcpy(dsVotes[i],"\0");
    }

    PDM = malloc(msgCount * sizeof(char*));
    for(i=0;i<msgCount;i++)
    {
        PDM[i] = malloc(maxLength*sizeof(char));
    }

    //no cool way to load them
    strcpy(PDM[0],"`Hell, yeah                  \0");
    strcpy(PDM[1],"`Surely you can't be serious \0");
    strcpy(PDM[2],"`I am serious                \0");
    strcpy(PDM[3],"`and don't call me Shirley   \0");
    strcpy(PDM[4],"`Vote for chat program??     \0");
    strcpy(PDM[5],"`there's our agreement       \0");
    strcpy(PDM[6],"`frak this!                  \0");
    strcpy(PDM[7],"`Help me, help you           \0");
    strcpy(PDM[8],"`OMG, The rain is wet!!!     \0");
    strcpy(PDM[9],"`Bond, James Bond            \0");

    messageStatus = malloc(maxUsers * sizeof(int));

    //message linked list
    linkedListInit();

    //print stats
    printf("Starting server\nPort: %i\nMax Users of: %i\nMax UserName Length: %i\n", port, maxUsers, UNAMELENGTH);

    struct sockaddr_in sAddr;
    int listensock;
    int newsock;
    int result;
    pthread_t thread_id;
    int val;

    listensock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    val = 1;
    result = setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if(result < 0)
    {
        perror("server");
        return 0;
    }


    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(port);
    sAddr.sin_addr.s_addr = INADDR_ANY;

    if(bind(listensock, (struct sockaddr*) &sAddr, sizeof(sAddr)) < 0)
    {
        perror("server");
        return 0;
    }

    if(listen(listensock, 10) < 0)
    {
        perror("server");
        return 0;
    }

    int startLinkedList = 1;

    while(1)
    {
        //printf("SERVER: Now Accepting Clients \n");

        newsock = accept(listensock, NULL, NULL);

        if(pthread_create(&thread_id, NULL, thread_proc, (void*) newsock) != 0)
        {
            perror("server");
        }


        pthread_detach(thread_id);
        sched_yield();

        //test code
        for(i=0; i<maxUsers; i++)
        {
            printf("Userid: %i    Name: %s\n",i, userNames[i]);
        }

        usleep(500000); //sleep half a second

        if(startLinkedList == 1)
        {//only to be ran on startus
            startLinkedList = 0;
            nextNode();
        }

    }

    return(EXIT_SUCCESS);
}

void* thread_proc(void *arg)
{
    int sock;
    int id=0;
    char cIBuffer[sizeof(ConnectInit)];
    time_t start,lastSeen, now; 
    

    sock = (int) arg;

    //the ConnectINIT part
    recv(sock, cIBuffer, sizeof(ConnectInit), 0);
    ConnectInit * cI = &cIBuffer;
    char * name = cI->userName;


    //form ACK response
    ConnectACK ack;
    ack.id = 0;
    ack.status = 0;

    printf("User with name:%s attempted to join\n", name);
    
    //handle new user bad status    
    if(numUsers >= maxUsers)
    {
        printf("WARNING you have too many users UserName:%s  Current Users: %i\n",name,numUsers);
        ack.status = 1;
    }
    else if(checkDupUserName(name))
    {
        printf("ERROR: duplicate username found\n");
        ack.status = 2;
    }
    else if(strlen(name) == 0 && name[0] != ' ')
    {
        printf("ERROR: username is blank or length of zero\n");
        ack.status=4;
    }
    else
    {
        sem_wait(&lusers); 
        numUsers++;
        totalUsers++;
        id = setUserName(name);
        if(id == -1)
        {
            printf("Error getting an ID for user\n");
            ack.status = 3;
        }
        sem_post(&lusers);

        ack.id = id;
        //ack.status already set to 0 unless failed to get unique user id
    }

    send(sock, &ack, sizeof(ConnectACK), 0);

    if(ack.status != 0)
    { //if error occured will free resources asap
        close(sock);
        pthread_exit(0); 
    }
    
    //send yell messages (PDM) 
    char * tmpPDM = malloc(1500*sizeof(char));
    
    sprintf(tmpPDM,"!%4d`%i`%i",maxLength*msgCount,maxLength,msgCount);    
     
    int i;
    for(i=0;i<msgCount;i++)
    {
        strncat(tmpPDM,PDM[i],maxLength);
    }

    send(sock,tmpPDM,strlen(tmpPDM)-1,0);
    free(tmpPDM);

    printf("UserID : %i  Username:%s  Current Users: %i\n", id, name, numUsers);

    //user sussesfully connected and accepted by server, chat relay code
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock,&fds);
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 5000; //sleep half a second 

    messageStatus[id] = 0; //ready to relay messages
    int quit = 0;
    int errors = 0;

    //adding broadcast message
    char * joinBroadcast = (char*)malloc(80); 
    sprintf(joinBroadcast,"%i`%i`%i`%s%s",id,0,50,name," has joined");
    sem_wait(&lmessage);
    addMessage(joinBroadcast);
    sem_post(&lmessage);
    free(joinBroadcast);

    printf("Getting ready for chat loop for user id:%i\n", id);

    //clear values
    time(&start);
    time(&lastSeen);
    time(&now);
    long timeConnected;

    for(;;)
    {
        //fd_set and timeval should be considered undefined aftwer select
        fd_set tmpfds = fds;
        struct timeval ttmp = t;

        switch(select(sock+1,&tmpfds,NULL,NULL,&ttmp))
        {
            case -1:
                printf("Something broke, client id:%s\n",name);
            default:
                if(FD_ISSET(sock,&tmpfds))
                {
                    //move to static array for speed
                    Chat * ch = (Chat *)malloc(sizeof(Chat));
                    char * sChat = malloc(1500);
                    char * token = malloc(1500);

                    memset(sChat,0,50);

                    //get size then serialized data
                    recv(sock,sChat,5,0); 
                    if(sChat[0] != '!')
                    {
                        printf("Recv message did not start with proper marker\n");
                        errors++;

                        if(errors > MAXTRANSMISSIONERRORS)
                        {
                        free(ch);
                        free(sChat);
                            printf("User %s, %i has reaced error threshold: %i, disconnecting...\n",name,id,MAXTRANSMISSIONERRORS);
                            quit = 1;
                            break;
                        }
                        continue;
                    }


                    int sizeToRecv = atoi(sChat+1);
                    recv(sock,sChat,sizeToRecv,0);

                    time(&lastSeen);
                    strncpy(token,sChat,sizeToRecv+1);

                    char * delim = strtok(token,"`");
                    //ch->id = atoi(delim);
                    delim = strtok(NULL,"`");
                    ch->status = atoi(delim);
                    delim = strtok(NULL,"`");
                    delim = strtok(NULL,"`");


                    //if chat status is 1, the user is quiting
                    if(ch->status == SB_QUIT)
                    {
                        quit = 1;
                        free(ch);
                        free(sChat);
                        break;
                    }
                    else if(ch->status == SB_KEYBOARDUPDATE)
                    {
                        memset(token,0,1500);
                        time(&now);
                        timeConnected=difftime(now,start);
                        sprintf(token,"\f%ld.%ldm %i %s",timeConnected/60,((timeConnected%60)*10)/60,dsVotesForMe[id],name);
                        strcat(sChat,token);
                    }
                    else if(ch->status == 4 && numUsers > 1)
                    {
                        char voteFor[1500];
                        memset(voteFor,0,1500);
                        sem_wait(&lusers);
                        getList(voteFor,id);
                        sem_post(&lusers);
                        if(strlen(voteFor) > 0)
                        {
                            sprintf(sChat,"!%4d`%i`%i`50`%s",(int)strlen(voteFor)+11,id,4,voteFor); 
                            printf("sChat: %s     Size:%i\n",sChat,(int)strlen(sChat));
                            send(sock,sChat,strlen(sChat),0);
                        }

                        free(token);
                        free(ch);
                        free(sChat);

                        continue;
                         
                    }
                    else if(ch->status == 5)
                    {
                        int vote = atoi(delim);
                        char tmp[5];
                        dsVotesForMe[vote]=dsVotesForMe[vote]+1;
                        sprintf(tmp,",%i",vote);
                        strcat(dsVotes[id],tmp);
                        printf("Voted for %i\n",vote);
                        printf("All votes: %s\n",dsVotes[id]);

                        free(token);
                        free(ch);
                        free(sChat);

                        continue;
                    }

                    sem_wait(&lmessage);
                    addMessage(sChat);
                    sem_post(&lmessage);
                   
                    free(token);
                    free(ch);
                    free(sChat);
                }

                char * chatMsg = malloc(UNAMELENGTH+MESSAGELENGTH+5); 
                char * tmpSend = malloc(UNAMELENGTH+MESSAGELENGTH+5); 
                sem_wait(&lmessage);
                
                if(!getMessage(id,chatMsg))
                {//nothing to send, restart loop
                    sem_post(&lmessage);
                    free(chatMsg);

                    //timeout code
                    time(&now);
                    if(difftime(now,lastSeen) > timeout)
                    {
                        printf("Client #%i has not been since for  %.0f seconds. Disconnecting client\n",id,difftime(now,lastSeen));
                        quit=1;
                        break;
                    }

                    continue;
                }

                sem_post(&lmessage);

                int sent = 0;
                int targetSent = 0 ;
    
                snprintf(tmpSend,strlen(chatMsg)+5+1,"!%4d%s",(int)strlen(chatMsg),chatMsg); 
               
                targetSent = strlen(tmpSend);  
                send(sock,tmpSend,targetSent-sent,0);


                //if sents start getting garbles uncomment below
                //while(sent < targetSent)
                //{
                //    sent += send(sock,tmpSend,targetSent-sent,0); 
                //}

                printf("USER %s:%s\n",name,chatMsg);   

                //test if user needs to be voted out
                if((float)dsVotesForMe[id]/numUsers > .50f)
                {
                    printf("Userd id %i is getting voed out\n",id);
                    char ejected[50];
                    char sendEject[150];
                    memset(sendEject,0,150);
                    memset(ejected,0,50);
                    sprintf(ejected,"!%4d`%i`%i`50`%s",(int)strlen(ejected)+11,id,6,"EJECTED"); 
                    send(sock,ejected,strlen(ejected),0);

                    memset(ejected,0,50);
                    sprintf(ejected,"People spoke, %s got ejected!",name);
                    quit=1;
                    free(chatMsg);
                    free(tmpSend);
                    continue;
                }

                free(chatMsg);
                free(tmpSend);
        }



        if(quit == 1)
        {
            break;
        }
    }

    printf("USER IS EXITING\n");


    //quiting code
    close(sock);
    sem_wait(&lusers);
    numUsers--;
    dsVotesForMe[id] = 0;
    removeVotes(id);
    strcpy(userNames[id], "\0");
    messageStatus[id] = -1;
    sem_post(&lusers);

    printf("User id: %i disconnected, Remaining Users: %i\n", id, numUsers);
    pthread_exit(0);
}

int setUserName(char * name)
{
    int i;
    for(i=0;i<maxUsers;i++)
    {
        if(!strcmp(userNames[i], "\0"))
        {
            strcpy(userNames[i],name);
            return i;
        }
    }

    return -1; //means too many users (something majorly wrong) or username exists
}

int checkDupUserName(char * name)
{
    int i;
    for(i=0; i<maxUsers;i++)
    {
        if(!strcmp(userNames[i], name))
        {
            return 1;
        }
    }

    return 0; //no match 
}

void addMessage(char * msg)
{
    //add to linked list
    addNode(msg);
}

int checkRecipients()
{//return 0 there is still a client waiting to recieve msg
    int i;
    for(i=0; i<numUsers; i++)
    {
        if(messageStatus[i] == 0)
        {
            //printf("Not all recip have got the message\n");
            return 0;
        }
    }

    //exexution made it to here means all clients recieved
    //the message - need to reset messageStatus array
    //for next message
    for(i=0; i<numUsers; i++)
    {
        if(messageStatus[i] == 1)
        {//ignore -1 (dropped users)
            messageStatus[i] = 0;
        }
    }

    //printf("All recip have got the message\n");

    return 1;
}

int getMessage(int id, char * msg)
{
    //return 0;

    if(messageStatus[id] !=0)
    {//return 0 becuase nothing to do
        if(getLength() > 1 && checkRecipients())
        {
            //printf("Moving next node\n");
            nextNode();
        }
        return 0; 
    }
    else
    {//return 1 and copy message
        //printf("Did not get message, retrieveing!\n");

        messageStatus[id] = 1;
        //printf("GetNode():%s\n",getNode());
        strcpy(msg,getNode());
        return 1;
    }
}

int getList(char *msg, int id)
{
    int * votedFor = malloc(numUsers * sizeof(int));
    char tmp[50] = {0};
    int i;
    for(i=0;i<numUsers;i++)
    {
        votedFor[i]=0;
    }

    char votes[50];
    strcpy(votes,dsVotes[id]);
    
    if(strcmp(votes,"")!=0)
    {
        //breakup string and mark voes
        char * p = strtok(votes,",");
        while(p != NULL)
        {
            votedFor[atoi(p)] = 1;
            p = strtok(NULL,",");
        }

    }

    for(i=0;i<numUsers;i++)
    {
        if(votedFor[i] == 0)
        {
            sprintf(tmp,",%i-%s",i,userNames[i]);
            strcat(msg,tmp);
            memset(tmp,0,50);
        }
    }

    return 1;
}

int removeVotes(int id)
{
    char votes[50] = {0};
    strcpy(votes,dsVotes[id]);
    if(strcmp(votes,"")!=0)
    {
        //breakup string and mark voes
        char * p = strtok(votes,",");
        while(p != NULL)
        {
            dsVotesForMe[atoi(p)] = dsVotesForMe[atoi(p)]-1;
            p = strtok(NULL,",");
        }
    }

    return 1;
}


