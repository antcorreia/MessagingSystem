#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

#define BUFLEN 512	// Tamanho do buffer
//#define PORT 80	// UDP // Porto para recepção de clientes
//#define PORT_TCP 160	// TCP // Porto para recepção de admins

char multicast_ip[16];
int multicast_port;

int PORT, PORT_TCP;
char DATABASE_FILE[50];

void admin();
void process_admin(int admin_fd);
bool verificaUser(char username[]);
bool validate_credentials(char u[], char p[], char ip[], char *permissions);
bool getIP(char userID[], char* userIP);
void add_connected_client(struct sockaddr_in socket);
struct sockaddr_in get_socket_from_ip(char ip[]);

void erro(char *s) {
	perror(s);
	exit(1);
}


struct sockaddr_in connected_sockets[50]; // Guardar os clientes que se ligaram

int main(int argc, char *argv[]) {
	if (argc != 4) {
    		printf("server <port client> <port config> <database file>\n");
    		exit(-1);
  	}
  	
  	PORT = atoi(argv[1]);
	PORT_TCP = atoi(argv[2]);
	strcpy(DATABASE_FILE,argv[3]);
	
	// Test multicast
	multicast_port = 1000;
	strcpy(multicast_ip, "239.1.1.1");
	
	if(fopen(DATABASE_FILE,"r") == NULL){
		printf("Ficheiro de registos nao existe\n");
		exit(0);
	}

	
	if (fork() == 0) // TCP
		admin();
	
	// UDP
	struct sockaddr_in si_minha, si_outra;

	int s,recv_len;
	socklen_t slen = sizeof(si_outra);
	char buf[BUFLEN];
	char sent_message[BUFLEN];

	// Cria um socket para recepção de pacotes UDP
	if((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		erro("Erro na criação do socket");
	}

	// Preenchimento da socket address structure
	si_minha.sin_family = AF_INET;
	si_minha.sin_port = htons(PORT);
	si_minha.sin_addr.s_addr = inet_addr("10.90.0.2");

	// Associa o socket à informação de endereço
	if(bind(s,(struct sockaddr*)&si_minha, sizeof(si_minha)) == -1) {
		erro("Erro no bind");
	}
	printf("Server aberto: %s, porto %d (UDP)\n", inet_ntoa(si_minha.sin_addr), ntohs(si_minha.sin_port));
	while (1) {
		// Espera recepção de mensagem (a chamada é bloqueante)
		if((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1) {
	  		erro("Erro no recvfrom");
	  	}
	  	// Para ignorar o restante conteúdo (anterior do buffer)
		buf[recv_len]='\0';
		
		int operation; // N da operacao
		char contents[100]; // Conteudo
		
		printf("Recebi uma mensagem do sistema com o endereço %s e o porto %d\n", inet_ntoa(si_outra.sin_addr), ntohs(si_outra.sin_port));
		printf("Conteúdo da mensagem: %s\n" , buf);
		
		char *main_token = strtok(buf, ":");
		operation = atoi(main_token);
		
		main_token = strtok(NULL, ":");
		strcpy(contents, main_token);
		
		printf("Operacao: %d\n", operation);
		printf("Resto: %s\n", contents);
		
		if(operation == 0){ // LOGIN
			char username[100];
			char password[100];
			char permissions[100];
			
			char *token = strtok(contents," ");
	  		strcpy(username,token);
	  		token = strtok(NULL," ");
	  		strcpy(password,token);
	  		
	  		printf("Username: %s\n", username);
	  		printf("Password: %s\n", password);
			if(validate_credentials(username, password, inet_ntoa(si_outra.sin_addr), permissions)){
				printf("Valido\n");
				add_connected_client(si_outra); // Guardar o cliente
				
				
			}	
			else
				printf("Invalido\n");	
				
			printf("%s\n", permissions);
			strcpy(sent_message, permissions);
			sendto(s, sent_message, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &si_outra,sizeof(si_outra));
		}
		else if(operation == 1){ // Enviar uma mensagem (CLIENTE-SERVIDOR)
			char receiver_username[100];
			char receiver_ip[20]; // from getIP()
			char message_to_deliver[BUFLEN];
			char *token = strtok(contents," ");
			strcpy(receiver_username, token);
		
			token = strtok(NULL, "");
			strcpy(message_to_deliver, token);
			
			if(!getIP(receiver_username, receiver_ip)) // Obter IP atraves do username
				strcpy(receiver_ip,"\0");
			
			strcpy(sent_message, message_to_deliver);
			printf("Mes:%s\n", sent_message);
			printf("Ip:%s\n", receiver_ip);
			struct sockaddr_in si_receiver = get_socket_from_ip(receiver_ip); // Obter Socket atraves do IP
			if(strcmp(inet_ntoa(si_receiver.sin_addr),receiver_ip) == 0){
				char temp[BUFLEN * 2];
				sprintf(temp, "Mensagem recebida: %s", sent_message);
				strcpy(sent_message, temp);
				sendto(s, sent_message, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &si_receiver,sizeof(si_receiver));
			}
			else{
				sprintf(sent_message,"Utilizador %s nao esta disponivel", receiver_username);
				sendto(s, sent_message, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &si_outra,sizeof(si_outra));
			}
		}
		else if(operation == 2){ // Obter IP e porto UDP do cliente destino (P2P)
			char receiver_username[100];
			char receiver_ip[20]; // from getIP()
			strcpy(receiver_username, contents);
			
			getIP(receiver_username, receiver_ip); // Obter IP atraves do username
			struct sockaddr_in si_receiver = get_socket_from_ip(receiver_ip); // Obter Socket atraves do IP
			if(strcmp(inet_ntoa(si_receiver.sin_addr),receiver_ip) == 0){
				sprintf(sent_message,"%s %d", inet_ntoa(si_receiver.sin_addr), ntohs(si_receiver.sin_port));
				printf("P2P: %s\n", sent_message);
				sendto(s, sent_message, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &si_outra,sizeof(si_outra));
			}
			else{
				sprintf(sent_message,"Utilizador %s nao esta disponivel", receiver_username);
				sendto(s, sent_message, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &si_outra,sizeof(si_outra));
			}
		}
		else if(operation == 3){ // Obter endereco multicast (Grupo)
			sprintf(sent_message,"MULTICAST %s %d", multicast_ip , multicast_port);
			sendto(s, sent_message, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &si_outra,sizeof(si_outra));
		}
		else
			printf("Operacao invalida\n");
		
	
		
	}
	// Fecha socket e termina programa
	close(s);
	return 0;
}


bool validate_credentials(char u[], char p[], char ip[], char *permissions){
	FILE *fp;
	char line[100];
	char linesplit[6][100];
	char *token;
	// Open config file
  	fp = fopen(DATABASE_FILE, "r");
  	
  	
  	while(fgets(line, sizeof(line), fp) != NULL) {
  		int i = 0;
  		token = strtok(line," ");
  		while(token != NULL){
  			strcpy(linesplit[i++],token);
  			token = strtok(NULL," ");
  		}
  		if((strcmp(linesplit[0],u) == 0) && (strcmp(linesplit[2],p) == 0) && (strcmp(linesplit[1],ip) == 0) ){
  			sprintf(permissions, "%s %s %s", linesplit[3], linesplit[4], linesplit[5]);
  			return true;
  		}	
  	}
  	
  	fclose(fp);
  	sprintf(permissions, "Credenciais invalidas");
  	return false;
}

bool getIP(char username[], char* userIP){
    char line[100], ip[100], utilizador[100];
    FILE *fp = fopen(DATABASE_FILE, "r");

    while (fgets(line, sizeof(line), fp)){
        sscanf(line, "%s %s", utilizador, ip);
        if (strcmp(utilizador, username) == 0){
            fclose(fp);
            strcpy(userIP, ip); 
            return true;
        }
    }

    fclose(fp);
    return false;
}


void add_connected_client(struct sockaddr_in socket){
	for (int i = 0; i < 50;i++){
		if((strcmp(inet_ntoa(connected_sockets[i].sin_addr),"0.0.0.0") == 0) || (connected_sockets[i].sin_addr.s_addr == socket.sin_addr.s_addr)){
			connected_sockets[i] = socket;
			break;
		}
	}
}

struct sockaddr_in get_socket_from_ip(char ip[]){
	for (int i = 0; i < 50;i++){
		if(strcmp(inet_ntoa(connected_sockets[i].sin_addr),ip) == 0){
			return connected_sockets[i];
		}
	}
	return connected_sockets[0]; // evitar erros
}






void admin(){
	int fd, admin;
	struct sockaddr_in addr, admin_addr;
	int admin_addr_size;
	

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("10.90.0.2");
	addr.sin_port = htons(PORT_TCP);
	

	if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		erro("na funcao socket");	
	if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
		erro("na funcao bind");
	if( listen(fd, 5) < 0)
		erro("na funcao listen");
	admin_addr_size = sizeof(admin_addr);

	printf("Server aberto: %s, porto %d (TCP)\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	while (1) {
		//clean finished child processes, avoiding zombies
		//must use WNOHANG or would block whenever a child process was working
		while(waitpid(-1,NULL,WNOHANG)>0);
		//wait for new connection
		admin = accept(fd,(struct sockaddr *)&admin_addr,(socklen_t *)&admin_addr_size);
		if (admin > 0) {
      			if (fork() == 0) {
        			close(fd);
        			printf("Admin connecting from (IP:port) %s:%d\n", inet_ntoa(admin_addr.sin_addr), admin_addr.sin_port);
        			process_admin(admin);
        			printf("Admin disconnected from server\n");
        			
        			exit(0);
      			}
		close(admin);
		}

	}
  	exit(0);
}

void process_admin(int admin_fd){
	char admin_message[BUFLEN];
	
	char buffer[BUFLEN];
	buffer[0] = '\0';

	while(strcmp(buffer, "QUIT") != 0){
		admin_message[0] = '\0';
    		read(admin_fd, buffer, BUFLEN-1);
    

    		if(strcmp(buffer, "LIST") == 0){ // LIST USERS
    			FILE *fp = fopen(DATABASE_FILE, "r");
    			rewind(fp);
    			char line[100];
    		
    			// Ler ficheiro de registos
    			strcpy(admin_message, "User-id | IP | Password | Cliente-Servidor | P2P | Grupo\n");
    			while(fgets(line, sizeof(line), fp) != NULL) {
    				strcat(admin_message,line);
    			}
    			fclose(fp);
    			write(admin_fd, admin_message, sizeof(admin_message));
		}
		else if(strncmp(buffer, "ADD", 3) == 0){ // ADD USER
			char username[100];
			char line[100];
			sscanf(buffer, "ADD %s", username);
			char *token = strtok(buffer, " ");
			token = strtok(NULL, "");
			strcpy(line, token);
	  		
			if(verificaUser(username)){ // Verifica se ja existe
				FILE *fp = fopen(DATABASE_FILE, "a");
				
				strcat(line, "\n");
				fputs(line, fp);
				fclose(fp);
				strcpy(admin_message, "Utilizador adicionado");
				write(admin_fd, admin_message, sizeof(admin_message));
			}
			else{
				strcpy(admin_message,"Utilizador ja existe");
				write(admin_fd, admin_message, sizeof(admin_message));
			}
		}
		else if(strncmp(buffer, "DEL", 3) == 0){ // DELETE USERS
			char username[100];
			char line[100];
			char new_username[100];
			int line_number = 0;
			sscanf(buffer, "DEL %s", username);
			
			if(!verificaUser(username)){ // Verifica se ja existe
				FILE *fp = fopen(DATABASE_FILE, "r");
				FILE *fp2 = fopen("database_temp.txt", "w+");
				
				// Get line of username
				while(fgets(line, sizeof(line), fp) != NULL){
					sscanf(line, "%s", new_username);
					if(strcmp(new_username, username) == 0)
						break;
					else
						line_number++;	
				}
				rewind(fp);
				// Create new file without line
				while(fgets(line, sizeof(line), fp) != NULL){
					if(line_number != 0){
						fputs(line, fp2);
					}
					line_number--;
				}
				fclose(fp);
				fclose(fp2);
				
				remove(DATABASE_FILE);
				rename("database_temp.txt", DATABASE_FILE);
				strcpy(admin_message,"Utilizador apagado");
				write(admin_fd, admin_message, sizeof(admin_message));
				
			}
			else{
				strcpy(admin_message,"Utilizador nao existe");
				write(admin_fd, admin_message, sizeof(admin_message));
			}
		
		}

		printf("Admin sent command!\n");
		
	}
	strcat(admin_message,"A desligar...\n");
	write(admin_fd, admin_message, sizeof(admin_message));
}

bool verificaUser(char username[]){
    FILE *fp = fopen(DATABASE_FILE, "r");
    char line[100], readUsername[100];

    while(fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%s", readUsername);
        if (strcmp(username, readUsername) == 0)
            return false;
    }

    fclose(fp);
    return true;
}


