/****************** SERVER CODE ****************/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/file.h>
#include <unistd.h>
#include <arpa/inet.h>



#define MAX_NUM_OF_STATION 100

#define STREAM	0
#define DEBUG	   printf //uncomment to remove all printf's
#define FRAME_SIZE 1024
#define PORT 2500


#define HELLO 		0		
#define ASK_SONG 	1
#define UP_SONG 	2 

#define WELCOME 0
#define PREMIT_SONG 2

#define COM_TYPE	0
#define STAEION_NUM	1
#define SONG_SIZE	1
#define SONG_NAME_SIZE	5
#define SONG_NAME	6	
#define WELCOME_SIZE 9

#define  REPLY_TYPE 	0
#define  NUM_STATION	1
#define  M_GROUP	3
#define  PORT_NUM	7

#define ANNOUNCE 	1
#define NAME_SIZE 	1
#define SONG_AN_NAME 2

#define NEW_STATION 4
#define INVALIDCOMMAND 3;
/* -----------Union----------- */

typedef union _group_16{
	unsigned char u8[2];
	short u16;
} group_16;


typedef union _group_32{
	unsigned char u8[4];
	int u32;
} group_32;

/*-------------------------------*/



/* handle the number of client in the server
--------------------------------------
in this program the server hold a static 
varible name " stations "

to use this struct use the fallowing function"

	void init_socket_array();
	void add_socket(int newSocket);
	void rmv_socket(int newSocket);
*/
typedef struct dynamic_socket{  
	int num_of_socket;
	int size;
	int *socket_array;
}dynamic_socket;


/* handle the number of client uploading song
---------------------------------------------

to use this struct use the fallowing function:
	void init_Linkls();
	int add_next(int socket,char* name, int num_of_byte);
	int remove_ls(int socket);
	void LS_size();
*/
typedef struct linkls{

	struct linkls * next;
	int socket;
	FILE *fd  ;
	char name[200];
	int num_of_byte;
	unsigned char name_size;

}linkls;




/*handle the number of station and their song name
---------------------------------------------------

	void add_station(char * song_name)
	unsigned char getSongName(char* name,short station)
*/
typedef struct{

	int num_of_station;
	char * song_name[100];

}dynamic_station;

typedef struct{
	int sd;
	FILE *songFD;
	char *songName;
	//int numOfStation;
	//listener list[100];
}station;





//static int num_of_client;
static group_32 mulyicastGroup;
static group_16 port_num;
static dynamic_socket socket_struct;
static dynamic_station stations;
static  fd_set readfds;
static linkls Linkls;

struct sockaddr_in tmpIP;
struct in_addr iaddr;
struct sockaddr_in saddr;
struct timespec waitTime;
int sd,i,numOfFrames,len;
int fileSize = 50000;
FILE *fd;
char databuf[1024];
int len;
unsigned char TTL = 128;
char one = 1;
int cn = 0,ssent,i=0;
FILE *fp;
station stations_radio[100];
int songsNumber = 0;
struct timeval timeout;

char DST_IP[50] = "239.0.0.1";
short DST_PORT = 6000;
short DST_TCP_PORT;


void finish(char* s){
	perror(s);
	exit(1);
}





void make_Wellcom_p(char *buffer);
int make_Song_p(unsigned char *buffer, short station);
unsigned char getSongName(char* name,short station);
void Apllication_function(int newSocket);
void init_socket_array();
void add_socket(int newSocket);
void rmv_socket(int newSocket);
linkls * find(int socket);
int add_next(int socket,unsigned char song_name_size,char* name, int num_of_byte);
int remove_ls(int socket);
void LS_iteam();
void print_sockets();
void send_inv(int Socket,char *invM);
int user_input();
void print_station();
void add_new_station(char* songName);
void *radio_stream();
void init_Linkls();
void make_Wellcom_p(char *buffer);
void add_station(char* song_name );
void resetTimer();
group_32 ip_to_group32(char * addr);
void 	print_address();




int main(int argc, char * argv[] ){

	char ip[50] ={0};
	int welcomeSocket, newSocket,i;
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;
	pthread_t Stream;

	if (argc < 4) {
	    printf("./radio_server <tcpport> <multicastip> <udpport> <file1> <file2> ... \n");
	    exit(1);
	}
	strcpy(DST_IP,argv[2]);
	DST_PORT = atoi(argv[3]);
	
	strcpy(ip,argv[2]);
	mkdir("MP3_FILE", 0777);
	init_socket_array();   // init the socket_struct
	init_Linkls();
	stations.num_of_station = 0;

	mulyicastGroup = ip_to_group32(ip);
	if(mulyicastGroup.u32 == -1){
		printf("invalid multicast ip try agin \n");
		return 1;
	}

	port_num.u16 = DST_PORT;
	DST_TCP_PORT = atoi(argv[1]);

    pthread_create(&Stream, NULL, radio_stream, NULL);

	welcomeSocket = socket(PF_INET, SOCK_STREAM, 0);
	add_socket(welcomeSocket);	 // add the welcome socket

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(argv[1])); // set TCP port
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//rverAddr.sin_addr.s_addr = inet_addr("127.1.1.1");
	//serverAddr.sin_addr.s_addr = inet_addr("132.72.105.95");

	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

	bind(welcomeSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

	if(listen(welcomeSocket,5)==0){
		printf("  ccccc   ssss   EEEEEE  \n");
		printf(" cc      ss      E       \n");
		printf(" cc        ss    EEEEEE  \n");
		printf(" ccc        sss  E     	 \n");
		printf("  ccccC  sssss   EEEEEE  \n");
	}
	else
		printf("Error\n");

	if(argc > 4)
		for(i = 4 ;i < argc ;i ++)
			add_station(argv[i]);
		
	START:
	printf("Enter:\n1. 'p' to see all the data bases of the available stations\n2. 'q' to quit the program\n");

	//main loop 
	while(1){
		resetTimer();
		if(select(20,&readfds,NULL,NULL,NULL/*&timeout*/)<0){     	// wait for select signal
			perror("select");
			goto CLOSE;
		}
		//DEBUG("select\n");

		if(FD_ISSET(welcomeSocket,&readfds)){   		// check if new welcomeSocket
			newSocket = accept(welcomeSocket, (struct sockaddr *) &serverStorage, &addr_size);
			if(newSocket == -1)
			perror("faild to open new sockt: ");	
			else
			add_socket(newSocket);  			// add the new socket
		}


		for(i=1;i < socket_struct.num_of_socket;i++)      		 //check if new client message
			if(FD_ISSET(socket_struct.socket_array[i],&readfds))
				Apllication_function(socket_struct.socket_array[i]); // answer to client message  


		if(FD_ISSET(STREAM,&readfds)){

				if(user_input()==-1)
					goto CLOSE;
					goto START; }
	}//while

	CLOSE:

	printf("Bye Bye..:) \n");

	/*
	close_sock(); TODO
	close_ls();
	clos_station();
	*/

	pthread_cancel(Stream);

	for(i=0;i<songsNumber;i++)
		close(stations_radio[i].sd);


	for(i=0;i < socket_struct.num_of_socket;i++)       //case of error
		close(socket_struct.socket_array[i]);

	free(socket_struct.socket_array);
	return 0;
}//main


//sand the application packet regard to the client massage
//set buffer with ask song packet

/* -----------  Radio stream --------------  */


//radio_stream
void *radio_stream(){

int i;
songsNumber = 0;
//DEBUG("enetr to radio_stream\n");

	while(1)
		{

			if(songsNumber < stations.num_of_station ) // if there is new station
			{
				add_new_station(stations.song_name[songsNumber]);
			}

			for(i = 0;i < songsNumber; i++)
			{
				if(feof(stations_radio[i].songFD))
					rewind(stations_radio[i].songFD);	//if the song is over, start over

				tmpIP.sin_addr.s_addr = (inet_addr(DST_IP)+htonl(i));// saddr.sin_addr.s_addr + htonl(i);

				if((len = fread(databuf,1,1024,stations_radio[i].songFD)) < 0)
					finish("failed to read the file !!\n");

				if(sendto(stations_radio[i].sd,databuf,len,0,(struct sockaddr*)&tmpIP,(size_t)sizeof(tmpIP)) == -1)
					finish("failed to send song !!\n");
			}//for

			waitTime.tv_sec=0;
			waitTime.tv_nsec = 62500000;
			nanosleep(&waitTime,NULL);
		}//while



}//radio_stream
void add_new_station(char* songName){

char buf[200];
//DEBUG("add_new_station\n");

	// set content of struct saddr and imreq to zero
	memset((char*)&saddr, 0, sizeof(struct sockaddr_in));
    memset(&iaddr, 0, sizeof(struct in_addr));
	memset((char*)&tmpIP, 0, sizeof(struct sockaddr_in));

	snprintf(buf, 200, "MP3_FILE/%s",songName);
	if(!((stations_radio[songsNumber]).songFD = fopen(buf,"rb")))
		finish("failed to open file \n");
	else
		printf("file opened \n");

	saddr.sin_family = PF_INET;
   	saddr.sin_port = htons(0); // Use the first free port
 	saddr.sin_addr.s_addr = htonl(INADDR_ANY); // bind socket to any interface

	//Temp sockaddr struct
	tmpIP.sin_addr.s_addr = saddr.sin_addr.s_addr;
	tmpIP.sin_family = PF_INET;
	tmpIP.sin_port = htons(0);


	//Open a UDP socket
	if((stations_radio[songsNumber].sd = socket(PF_INET,SOCK_DGRAM,0)) < 0)
		finish("Error opening socket");
	else
		printf("Opening the datagram socket...OK.\n");


	if(bind(stations_radio[songsNumber].sd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == -1)
		finish("Error binding socket to interface");
	else
		printf("bind to interface...ok\n");


	iaddr.s_addr = INADDR_ANY; // use DEFAULT interface

	// Set the outgoing interface to DEFAULT
   	setsockopt(stations_radio[songsNumber].sd, IPPROTO_IP, IP_MULTICAST_IF, &iaddr, sizeof(struct in_addr));

	// Set multicast packet TTL to 255
	setsockopt(stations_radio[songsNumber].sd, IPPROTO_IP, IP_MULTICAST_TTL, &TTL, sizeof(TTL));

	// set destination multicast address
   	tmpIP.sin_family = PF_INET;
   	tmpIP.sin_addr.s_addr = inet_addr(DST_IP)+htonl(songsNumber);//saddr.sin_addr.s_addr + htonl(songsNumber);
   	tmpIP.sin_port = htons(DST_PORT);


	// join the multicast group
   	setsockopt(stations_radio[songsNumber].sd, IPPROTO_IP, IP_ADD_MEMBERSHIP,DST_IP, 15);


	songsNumber++;

}

/*-------------------------------------------*/

void Apllication_function(int newSocket){

	unsigned char buffer[1050] ={0};
	char song_name[200];
	unsigned char song_name_size,premit;
	group_16 num_16 ={0};
	group_32 song_size = {0};
	int i,rcv_f;
	linkls * socket;

		//DEBUG("application\n");
		rcv_f = recv(newSocket, buffer, 1024, 0);
		if(rcv_f<= 0)
		{
			if(rcv_f<0)
			perror("receive error");

			else
			printf("finack -rm: %d \n",newSocket);


			socket = find(newSocket);
			if(socket!=NULL)            //if the socket is closing while is uploading file delete the file
			{
				snprintf(song_name, sizeof(song_name), "MP3_FILE/%s",socket->name);
				remove(song_name);

			}

			rmv_socket(newSocket); //TODO if process is uploading delete the unfinish file
			return;
		}



		socket = find(newSocket);
		if(socket != NULL) // if the socket is sanding song data
		{
			socket->num_of_byte -= rcv_f;

				for(i = 0;i< rcv_f ; i++)
					fputc(buffer[i], socket->fd);

			if(socket->num_of_byte == 0)
			{
				//print_station();
				//DEBUG("%s\n",socket->name);

				add_station(socket->name);
				remove_ls(socket->socket);

				num_16.u16 = stations.num_of_station;
				buffer[0] = NEW_STATION;
				buffer[1] = num_16.u8[1];
				buffer[2] = num_16.u8[0];


				for(i=1;i < socket_struct.size;i++)    //init readfds
				{
					if(socket_struct.socket_array[i]!=0)
					{
						send(socket_struct.socket_array[i],buffer,3,0); //send new station
							if(i == -1)
								perror("send error");
					}//if send error

				}//for

			}//if the end of reciving
		}//if find

		else
		{

		switch(buffer[COM_TYPE]){

		case HELLO:	
									  //Hello
			//DEBUG("HELLO\n");
			make_Wellcom_p(buffer);
			i = send(newSocket,buffer,WELCOME_SIZE,0);
			if(i == -1)
			{
				perror("receive error");
				break;
			}

			break;


		case ASK_SONG:	  								  //Ask_song

			//DEBUG("ASK_SONG\n");
			num_16.u8[1] = buffer[0+STAEION_NUM];
			num_16.u8[0] = buffer[1+STAEION_NUM];

			i = make_Song_p(buffer,num_16.u16);
			i= send(newSocket,buffer,i,0);
			if(i == -1)
			{
				perror("receive error");
				break;
			}
         	break;


		case UP_SONG:                                       //Upsong

			//DEBUG("UP_SONG\n");
				
			//1. find if the client uplouading allready // cheak in the socket array flag 
			//2. if the secound byte equal to zero do 2 3 else do 4
			//3. open the file and add the buffer data to it and return
			//4. close the file and sand ip massage to the other procces and clear the fd file from the array

			//socket = find(newSocket);
			//if(socket == NULL) // case this is new ask
			//{


			//	for(i = 0 ;i<4;i++)
					song_size.u8[0] = buffer[3+SONG_SIZE];
					song_size.u8[1] = buffer[2+SONG_SIZE];
					song_size.u8[2] = buffer[1+SONG_SIZE];
					song_size.u8[3] = buffer[0+SONG_SIZE];


				for(i = 0 ;i<1;i++)
					song_name_size = buffer[i+SONG_NAME_SIZE];

				//DEBUG("UP_SONG name size %d",song_name_size);

				for(i = 0 ;i<song_name_size;i++)
					song_name[i] = buffer[i + SONG_NAME];
					song_name[i] = '\0';

					/*
				DEBUG("strlen %d  song name size %d",strlen(song_name),song_name_size);
				 if(strlen(song_name) != song_name_size){
					send_inv(newSocket,"\n ERROR :) SongNameSize field isn't equal to the size of the song name\n");
					DEBUG("UP_SONG error 1\n");
					rmv_socket(newSocket);
					DEBUG("UP_SONG error 2\n");

				    break;
				 }
				*/

				premit = 1; //init premiut
				if(add_next(newSocket,song_name_size,song_name,song_size.u32) == -1) // case it secssed add the new
					{
					premit = 0;
					perror("add_next :");
					}

				buffer[0] = PREMIT_SONG; //2
				buffer[1] = premit;
				send(newSocket,buffer,2,0); //sand premit massge

							if(i == -1)
							{
								perror("send error");
								break;
							}

			//}
			break;

			// Upload_song(song_size.u32,song_name_size,song_name);

		default:

			//DEBUG("default\n");
			printf("unknown massage type closing TCP connection");
			rmv_socket(newSocket);

		}//switch
	}//else

}//Apllication_function
int user_input(){

char buffer[200];

	memset((void*) buffer,0,200);
	read(STREAM,(void*)buffer,200);

	   //printf("%s\n",buffer);

	   switch(buffer[0]){

	   case 'q':
	   case 'Q':
		   return -1;

	   case 'p':
	   case 'P':
		   	print_address();
		    print_sockets();
		    print_station();
		    //LS_iteam();
		    break;

	   default:
		   printf("unknown input\n");

	   }


	return 1;
}//user_input
//set buffer for
void make_Wellcom_p(char *buffer){
	group_16 num_16;
	int i;

	buffer[REPLY_TYPE] = WELCOME; //0

	num_16.u16 = stations.num_of_station;

	//for(i = 0;i<2;i++)
		buffer[0+NUM_STATION] = num_16.u8[1]; // 1
		buffer[1+NUM_STATION] = num_16.u8[0]; // 1

	//for(i = 0 ;i<4;i++)
		buffer[0+M_GROUP] = mulyicastGroup.u8[3]; //3
		buffer[1+M_GROUP] = mulyicastGroup.u8[2]; //3
		buffer[2+M_GROUP] = mulyicastGroup.u8[1]; //3
		buffer[3+M_GROUP] = mulyicastGroup.u8[0]; //3

	//for(i = 0 ;i<2;i++) //7
		buffer[0+PORT_NUM] = port_num.u8[1];
		buffer[1+PORT_NUM] = port_num.u8[0];


}//make_Wellcom_p
int make_Song_p(unsigned char *buffer, short station){

	unsigned char song_name_size;
	char ans[20] = "No souch station";

	buffer[REPLY_TYPE] = ANNOUNCE; //1
	if(station <= stations.num_of_station-1 && station >=0 )
	{
		song_name_size  = strlen(stations.song_name[station]);
		buffer[NAME_SIZE] = song_name_size;
		//snprintf(buffer + SONG_AN_NAME, song_name_size +1, "%s",(char *)stations.song_name[station]);
		strcpy((char *)buffer + SONG_AN_NAME ,stations.song_name[station]);
	}//if
	else
	{
		song_name_size  = strlen(ans);
		buffer[NAME_SIZE] = song_name_size;
		snprintf((char * )buffer + SONG_AN_NAME, 200, "%s",ans);
		//strcpy((char *)buffer + SONG_AN_NAME, ans);
	}//else
		return song_name_size + 2;

}//make_Song_p

/* handle the number of client in the server
--------------------------------------
in this program the server hold a static 
varible name " stations "

to use this struct use the fallowing function"

	void init_socket_array();
	void add_socket(int newSocket);
	void rmv_socket(int newSocket);
*/

void init_socket_array(){

	socket_struct.num_of_socket = 0;
	socket_struct.size = 5;
	socket_struct.socket_array =(int *)calloc(socket_struct.size,sizeof(int));

}
void add_socket(int newSocket){
	int i;

	if(socket_struct.num_of_socket+1 >= socket_struct.size ) // if there is need to make some more room in the array
	{
		socket_struct.socket_array  =(int *)realloc(socket_struct.socket_array,(socket_struct.size+5)*sizeof(int)); // malloc new space
		socket_struct.size = socket_struct.size+5;

		for(i=socket_struct.num_of_socket;i<socket_struct.size;i++) //
			socket_struct.socket_array[i] = 0;
	}

	//find empty space

	for(i=0;i<socket_struct.size ;i++)
		if(socket_struct.socket_array[i] == 0)
			break; 


	socket_struct.num_of_socket = socket_struct.num_of_socket+1;       //incremant num of socket 
	socket_struct.socket_array[i] = newSocket;	           //place the new socket in the empty place
	//FD_SET(newSocket,&readfds);

	//print_sockets();
	//LS_iteam();

}//add
void rmv_socket(int newSocket){
	int i;
	int *new,*temp;

	remove_ls(newSocket);  // cheak if there is a file


	if(socket_struct.num_of_socket>0)
	{
		for(i=0;i<socket_struct.size ;i++) //find the socket to rmv
			if(socket_struct.socket_array[i] == newSocket)
			{
				socket_struct.socket_array[i] = 0;
				close(newSocket);
				socket_struct.num_of_socket = socket_struct.num_of_socket-1;

				//if the num of socket is less then the size-10 then shrink the size by 5
				if(socket_struct.num_of_socket < socket_struct.size-5)
				{
					new  =(int *)calloc(socket_struct.size-5,sizeof(int));

					temp = new;
					for(i=0;i<socket_struct.size ;i++) //do hard copy to the new location
						if(socket_struct.socket_array[i]!=0)
							*temp++ = socket_struct.socket_array[i];

					free(socket_struct.socket_array); //free the old location
					socket_struct.socket_array = new; //point to the new location

					socket_struct.size = socket_struct.size-5;
				}//if
			}//if
	}//if	
	else printf("no argument to remove\n");
	//print_sockets();
	//DEBUG(" \n");
	//LS_iteam();


}//rmv_socket
void print_sockets(){
	int i;


	printf("|CLIENT IN THE SYSTEM : %d \n",socket_struct.num_of_socket -1);
	printf("-----------------------------------------\n");
	printf("|#	|NUM		\n");


	for(i=1;i<socket_struct.size;i++){
		if(socket_struct.socket_array[i]!=0)
		printf("|%d	|%d	\n",i,socket_struct.socket_array[i]);	
	}
	printf("-----------------------------------------\n\n");

}

//this methode get name of song and reurn the station number
void add_station(char* song_name ){
int i;
i = strlen(song_name);

if(stations.num_of_station +1 < MAX_NUM_OF_STATION)
	{
	stations.song_name[stations.num_of_station] = (char *)malloc(i*sizeof(char*));
	strcpy(stations.song_name[stations.num_of_station],song_name);
	stations.num_of_station ++;
	return;
	}
printf("cant add more station");
}//add_station 

unsigned char getSongName(char* name,short station){

strcpy(name,stations.song_name[station]);
return strlen(name);

}//getSongName
void print_station(){
	int i;

	printf("| NUMBER OF STATION :%d   \n",stations.num_of_station);
	printf("-----------------------------------------\n");
	printf("|#	|SONG NAME	|\n");
	for(i=0;i<stations.num_of_station;i++)
		printf("|%d	|%s	| \n",i,stations.song_name[i]);
	printf("-----------------------------------------\n\n");


}

/* handle the number of client uploading song
---------------------------------------------

to use this struct use the fallowing function:

	void init_Linkls();
	int add_next(int socket,char* name, int num_of_byte);
	int remove_ls(int socket);
	void LS_size();
*/

void init_Linkls(){

	Linkls.next = NULL;
    Linkls.socket = 0;
	Linkls.fd = 0;
	Linkls.num_of_byte = 0;
	Linkls.name_size = 0;
}

int add_next(int socket,unsigned char song_name_size,char* name, int num_of_byte){

char buf[200] ;
FILE *temp;
linkls *current,*add;

 add = (linkls*)calloc(1,sizeof(linkls)); //case fiald to calloc
 if(add == NULL )
 return -1;

strcpy(add->name,name); // copy the name to station struct
snprintf(buf, sizeof(buf), "MP3_FILE/%s",name); // open new file with the name of the station
temp = fopen( buf,"w"); //case fiald to open file
	if(temp == NULL)
	{
		free(add);
		perror("file");
		return -1;
	}


add->fd = temp ;
add->num_of_byte = num_of_byte;
add->socket = socket;
add->next = NULL;
add->name_size = song_name_size;


current = &Linkls;
while(current->next != NULL)
	current = current->next;

current->next = add;
return 1;
}//add_next
linkls * find(int socket){
linkls *current;

//DEBUG("socket %d\n",socket);
current = &Linkls;

while(current != NULL)
	{

	if(current->socket == socket)
		return current;

	current = current->next;

	}
return NULL;

}//find
int remove_ls(int socket){

//DEBUG("1\n");
linkls *current,*next;

current = &Linkls;
next = current->next;

if(next == NULL){
//DEBUG("2\n");
	goto END;
}




while(next != NULL){

//DEBUG("4\n");

if(next->socket == socket)
{
	//DEBUG("5\n");
	fclose(next->fd); //close file
	current->next = next->next;
	free(next);
	printf("remove socket\n");
	//print_sockets();
	//LS_iteam();


	return 1; }

current = current->next;
next = next->next;
}

END:
printf("no such socket\n");
return -1;

}//remove_ls

//for debug
void LS_size(){
	int i;
	linkls *current;
	current = &Linkls;

	i=0;
	while(current->next != NULL){
		current = current->next;
		i++;
	}

	printf("LS_size 0 %d\n",i);

}//num_of_ls
void LS_iteam(){
	int i;
	linkls *current;
	current = &Linkls;

	i=0;
	printf("\n");
	while(current != NULL){

		printf("index:	%d\n",current->socket);
		current = current->next;
		i++;
	}

	printf("LS_size %d\n",i-1);

}//num_of_ls
int find_s(char *buf){

	int i;

	for(i=0;i<stations.num_of_station;i++)
		if(strcmp(buf,stations.song_name[i])==0)
			return 1;

	return 0;
}
void send_inv(int Socket,char *invM){
	unsigned char buffer[20];


	buffer[0] = INVALIDCOMMAND;
	buffer[1] = strlen(invM);
	snprintf((char *) buffer+2,(int)buffer[1],"%s",invM);

	send(Socket,buffer,2+buffer[1],0);

}
void resetTimer()
{
	int i;
	FD_ZERO(&readfds); 					//clear the readfds
	for(i=0;i < socket_struct.size;i++)
	{   	//init readfds
		if(socket_struct.socket_array[i]!=0)
		FD_SET(socket_struct.socket_array[i],&readfds);
	}



	FD_SET(0,&readfds); //Stream

	timeout.tv_sec = 0;
	timeout.tv_usec = 1000*100;  		//100 ms
	return;
}

// this function turn string ip into union group 32
group_32 ip_to_group32(char * addr){
	group_32 ans;
	char temp[50];
	int i=0,j=0,num;


while(*addr != '\0' && i<4)
	{
	printf("%c\n",*addr);

		if(*addr == '.')
		{
			 num = atoi(temp);
			if(num > 255)
			{
				ans.u32 = -1;
				return ans;
			}
			ans.u8[i] = num;
			memset((void*) temp,0,50);
			i++; addr++; j=0;
		}
		temp[j] = *addr++; j++;
	}
num = atoi(temp);

if(num > 255)
{
	ans.u32 = -1;
	return ans;

}
ans.u8[i] = num;


return ans;
}//set_addr
void 	print_address(){

	printf("-------------------------------------------------------------\n");
	printf("|MULTICAST ADDR	|UDP PORT	|TCP PORT	|\n");
	printf("|%d.%d.%d.%d	|%d		|%d		|\n",mulyicastGroup.u8[0],mulyicastGroup.u8[1],mulyicastGroup.u8[2],mulyicastGroup.u8[3],DST_PORT,DST_TCP_PORT);
	printf("------------------------------------------------------------\n\n");


}

