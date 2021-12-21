/**********************************************************************
 * CLIENTE liga ao servidor (definido em argv[1]) no porto especificado
 * (em argv[2]), escrevendo a palavra predefinida (em argv[3]).
 * USO: >cliente <enderecoServidor>  <porto>  <Palavra>
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define BUFLEN 512    // Tamanho do buffer

int shmid;
struct sockaddr_in* p2p_socket;


void erro(char *msg);
void menu_admin(int fd);

int main(int argc, char *argv[]) {		
  char endServer[100];
  char message[BUFLEN];
  char message_sent[BUFLEN];
  int fd, recv_len;
  struct sockaddr_in addr;
  struct hostent *hostPtr;
  socklen_t slen = sizeof(addr);

  if (argc != 3) {
    printf("cliente <host> <port>\n");
    exit(-1);
  }
  
  if (strcmp(argv[2],"160") == 0){ // TCP
	strcpy(endServer, argv[1]);
	if ((hostPtr = gethostbyname(endServer)) == 0)
	erro("Não consegui obter endereço");

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	addr.sin_port = htons((short) atoi(argv[2]));

	if ((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
	  erro("socket");
	if (connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0) // Ligar socket do cliente a socket do servidor
	  erro("Connect");

  	menu_admin(fd);
  }	
  else{ // UDP

	  strcpy(endServer, argv[1]);
	  if ((hostPtr = gethostbyname(endServer)) == 0)
	    erro("Não consegui obter endereço");

	  bzero((void *) &addr, sizeof(addr));
	  addr.sin_family = AF_INET;
	  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	  addr.sin_port = htons((short) atoi(argv[2]));

	  if ((fd = socket(AF_INET,SOCK_DGRAM,0)) == -1)
	      erro("socket");
	      
	      
	  // Shared Memory    
	  if((shmid = shmget(IPC_PRIVATE,sizeof(struct sockaddr_in), IPC_CREAT| 0777)) == -1)
	  	erro("shared memory");
	  if((p2p_socket = (struct sockaddr_in*) shmat(shmid,NULL,0)) == (struct sockaddr_in*) - 1)
	  	erro("shared memory attach");
	  	
	  
	  // Login    
	  char username[100];
	  char password[100];
	  char credentials[256];
	  printf("Introduza as credenciais: <nome> <password>\n");
	  scanf("%s %s", username, password);

	  sprintf(credentials, "0:%s %s", username, password);

	  sendto(fd, credentials, sizeof(credentials), MSG_CONFIRM, (const struct sockaddr *) &addr,sizeof(addr));

	  // Espera recepção de mensagem (a chamada é bloqueante)
	  if((recv_len = recvfrom(fd, message, BUFLEN, 0, (struct sockaddr *) &addr, (socklen_t *)&slen)) == -1)
	    	erro("Erro no recvfrom");

	  // Para ignorar o restante conteúdo (anterior do buffer)
	  message[recv_len]='\0';


	  if(strcmp(message, "Credenciais invalidas") == 0){
	      	printf("%s\n", message);
		exit(0);
	  }
	  
	  // Permissoes
	  char c2s[4];
	  char p2p[4];
	  char grupo[4];
	  
	  sscanf(message, "%s %s %s", c2s, p2p, grupo);
	  printf("Permissoes:\n[1] Cliente-Servidor -> %s\n[2] P2P -> %s\n[3] Grupo-> %s\n", c2s, p2p, grupo);
	  
	  
	  if(fork() == 0){ // Receive normal messages
	  	while(1){
	  		// Espera recepção de mensagem (a chamada é bloqueante)
	  		if((recv_len = recvfrom(fd, message, BUFLEN, 0, (struct sockaddr *) &addr, (socklen_t *)&slen)) == -1)
	    			erro("Erro no recvfrom");
	    		message[recv_len]='\0';
	    		
	    		char temp[BUFLEN];	
	    		strcpy(temp, message);	
	    		
	    		if(strcmp(strtok(temp, " "),"MULTICAST") == 0){
	    			char multicast_ip[16];
	    			int multicast_port;
	    			char *token = strtok(NULL, "");
	    			sscanf(token, "%s %d", multicast_ip, &multicast_port);
				while((getchar()) != '\n');
				
				
				p2p_socket->sin_family = AF_INET;
	   			p2p_socket->sin_port = htons(multicast_port);
				p2p_socket->sin_addr.s_addr = inet_addr(multicast_ip);
				
				
				if(fork() == 0){

					int fd_multicast = socket(AF_INET, SOCK_DGRAM, 0);
					if (fd_multicast < 0) {
						erro("socket");
						return 1;
					}


					u_int yes = 1;
					if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){
						erro("Reusing ADDR failed");
						return 1;
					}

					struct sockaddr_in multicast_socket;
	  				memset(&multicast_socket, 0, sizeof(multicast_socket));
					multicast_socket.sin_family = AF_INET;
    					multicast_socket.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    					multicast_socket.sin_port = htons(multicast_port);


    					if (bind(fd_multicast, (struct sockaddr*) &multicast_socket, sizeof(multicast_socket)) < 0) {
        					erro("bind");
        					return 1;
    					}
				
					socklen_t slen_multicast = sizeof(multicast_socket);

					struct ip_mreq mreq;
					mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip);
					mreq.imr_interface.s_addr = htonl(INADDR_ANY);
					if (setsockopt(fd_multicast, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0){
						erro("setsockopt");
						return 1;
					}
					
					char message_multicast[BUFLEN];
					int recv_len_multicast;
					while(1){
						if((recv_len_multicast = recvfrom(fd_multicast, message_multicast, BUFLEN, 0, (struct sockaddr *) &multicast_socket, (socklen_t *)&slen_multicast)) == -1)
	    						erro("Erro no recvfrom");
	    						
	    					message_multicast[recv_len_multicast]='\0';
	    					
	    					printf("%s\n\n", message_multicast);
					}
				
				
				}

				
	    		
	    		
	    		}
	    		
	    		else if(strcmp(strtok(temp, " "),"Mensagem") != 0){ // Para ler informacao P2P
	    			char socket_ip[16];
	    			int socket_port;
	    		
				sscanf(message, "%s %d", socket_ip, &socket_port);
				while((getchar()) != '\n');
				
	    		
	    			p2p_socket->sin_family = AF_INET;
	   			p2p_socket->sin_port = htons(socket_port);
				p2p_socket->sin_addr.s_addr = inet_addr(socket_ip);
	    		}
	    		else
	  			printf("%s\n\n", message);
	  	}
	  	exit(0);
	  }
	  
	 
	  int tipo = 0;
	  while(tipo == 0){
	  	printf("Que tipo de conexao quer utilizar? (Escolha um dos numeros apresentados em cima)\n");
		scanf("%d", &tipo);
		while((getchar()) != '\n');

		switch(tipo){
			case 1:
				if (strcmp(c2s, "no") == 0){
					printf("Nao tem permissao para esse tipo de conexao\n");
					tipo = 0;
				}
				else
					printf("A iniciar conexao Cliente-Servidor...\n");
				break;
			case 2:
				if (strcmp(p2p, "no") == 0){
					printf("Nao tem permissao para esse tipo de conexao\n");
					tipo = 0;
				}
				else
					printf("A iniciar conexao P2P...\n");
				break;
			case 3:
				if (strcmp(grupo, "no") == 0){
					printf("Nao tem permissao para esse tipo de conexao\n");
					tipo = 0;
				}
				else
					printf("A iniciar conexao Grupo...\n");
				break;
			default:
				printf("Esse numero nao e valido\n");
				tipo = 0;
		}
	  }
	  
	  char mensagem[256];
	  if(tipo == 1){
	  	char utilizador_destino[100];
	  	printf("Introduza o username do destinatario: ");
	  	scanf("%s", utilizador_destino);
	  	while((getchar()) != '\n');
	  	while(1){
			printf("Introduza a mensagem a enviar: ");
	  		fgets(mensagem, sizeof(mensagem), stdin);
	  		mensagem[strlen(mensagem) - 1] = '\0';
	  		sprintf(message_sent, "1:%s %s", utilizador_destino, mensagem);
	  		
	  		sendto(fd, message_sent, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &addr,sizeof(addr));
	  		sleep(1);
	  	}
	  }
	  else if(tipo == 2){
	  	char utilizador_destino[100];
	  	printf("Introduza o username do destinatario: ");
	  	scanf("%s", utilizador_destino);
	  	while((getchar()) != '\n');
	  	
	  	sprintf(message_sent, "2:%s", utilizador_destino);
	  	sendto(fd, message_sent, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &addr,sizeof(addr));
	  	sleep(1);
	  	
	  	
	  	struct sockaddr_in teste_socket; // this is utterly retarded
	  	bzero((void *) &teste_socket, sizeof(teste_socket));
	  	teste_socket.sin_family = AF_INET;
	  	teste_socket.sin_addr = p2p_socket->sin_addr;
	  	teste_socket.sin_port = p2p_socket->sin_port;
	  	
	  	while(1){
			printf("Introduza a mensagem a enviar: ");
	  		fgets(mensagem, sizeof(mensagem), stdin);
	  		mensagem[strlen(mensagem) - 1] = '\0';
	  		
	  		char temp[BUFLEN * 2];
			sprintf(temp, "Mensagem recebida: %s", mensagem);
			strcpy(message_sent, temp);
	  		
	  		sendto(fd, message_sent, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &teste_socket,sizeof(teste_socket));
	  		
	  		sleep(1);
	  	}

	  }
	  else if(tipo == 3){
	  	strcpy(message_sent, "3:MULTICAST");
	  	sendto(fd, message_sent, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &addr,sizeof(addr));
	  	sleep(1);
	  	
		int fd_multi = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd_multi < 0) {
			erro("socket");
			return 1;
		}


		struct sockaddr_in addr_multi;
		memset(&addr_multi, 0, sizeof(addr_multi));
		addr_multi.sin_family = AF_INET;
		addr_multi.sin_addr = p2p_socket->sin_addr;
		addr_multi.sin_port = p2p_socket->sin_port;
	  	while(1){
	  		printf("Introduza a mensagem a enviar: ");
	  		fgets(mensagem, sizeof(mensagem), stdin);
	  		mensagem[strlen(mensagem) - 1] = '\0';
	  		
	  		char temp[BUFLEN * 2];
			sprintf(temp, "Mensagem recebida no Multicast: %s", mensagem);
			strcpy(message_sent, temp);
	  		

	  		sendto(fd_multi, message_sent, BUFLEN, MSG_CONFIRM, (const struct sockaddr *) &addr_multi,sizeof(addr_multi));
	  		
	  		sleep(1);
	  	
	  	
	  	}
	  
	  
	  }

	 
	   
	  
	  
	  // Fechar socket
	  close(fd);
  }
  
  // Fechar
  exit(0);
}

void erro(char *msg) {
  printf("Erro: %s\n", msg);
    exit(-1);
}

void menu_admin(int fd){
	char buffer[BUFLEN];
	buffer[0] = '\0';

	char comando[100], comandoSplit[100][100], comandoInteiro[100];
	int i, info;
	while(strcmp(comando, "QUIT") != 0){
		info = 0;
		buffer[0] = '\0';
		printf("Insira um comando: ");
		fgets(comando,sizeof(comando),stdin);
		comando[strlen(comando) - 1] = '\0';
		printf("Comando: %s\n", comando);

		strcpy(comandoInteiro, comando);
		i = 0;
		char *token = strtok(comando, " ");
		while (token != NULL){
			strcpy(comandoSplit[i++], token);
			token = strtok(NULL, " ");
		}

		if(strcmp(comandoSplit[0],"LIST") == 0){
			if (strcmp(comandoSplit[1], "\0") == 0){
				write(fd, comando, sizeof(comando));
			}
			else
				printf("Demasiados argumentos para a funcao LIST\n");
		}		
		else if(strcmp(comandoSplit[0],"ADD") == 0){
			if (strcmp(comandoSplit[6], "\0") == 0)
				printf("Poucos argumentos para a funcao ADD\n");
			else if (strcmp(comandoSplit[7], "\0") != 0)
				printf("Demasiados argumentos para a funcao ADD\n");
			else if (strcmp(comandoSplit[4], "yes") != 0 && strcmp(comandoSplit[4], "no") != 0)
				printf("Argumento n4 da funcao ADD tem de ser 'yes' ou 'no'\n");
			else if (strcmp(comandoSplit[5], "yes") != 0 && strcmp(comandoSplit[5], "no") != 0)
				printf("Argumento n5 da funcao ADD tem de ser 'yes' ou 'no'\n");
			else if (strcmp(comandoSplit[6], "yes") != 0 && strcmp(comandoSplit[6], "no") != 0)
				printf("Argumento n6 da funcao ADD tem de ser 'yes' ou 'no'\n");
			else{
				int checkIP1 = 0, checkIP2 = 0, checkIP3 = 0, checkIP4 = 0;
				char check;
				sscanf(comandoSplit[2], "%d.%d.%d.%d%c", &checkIP1, &checkIP2, &checkIP3, &checkIP4, &check);
				if ((checkIP1 == 0 && checkIP2 == 0 && checkIP3 == 0 && checkIP4 == 0) || check != '\0' || (checkIP1 > 255 && checkIP2 > 255 && checkIP3 > 255 && checkIP4 > 255))
			    		printf("Argumento n2 da funcao ADD tem de ser um IP valido\n");
				else{
					write(fd, comandoInteiro, sizeof(comandoInteiro));
				}	
			}
		}	
		else if(strcmp(comandoSplit[0],"DEL") == 0){
			if (strcmp(comandoSplit[1], "\0") == 0)
				printf("Poucos argumentos para a funcao DEL\n");
			else if (strcmp(comandoSplit[7], "\0") != 0)
				printf("Demasiados argumentos para a funcao DEL\n");
			else
				write(fd, comandoInteiro, sizeof(comandoInteiro));
		}
		else if(strcmp(comandoSplit[0],"QUIT") == 0){
			write(fd, comando, sizeof(comando));
			read(fd, buffer, BUFLEN-1);
			printf("Resposta:\n%s\n", buffer);
			exit(0);
		}
		else{
			printf("Comando inexistente\n");
			info = 1;
		}	
	
		for (i = 0; i < 100; i++)
			strcpy(comandoSplit[i], "\0");
			
		// Resposta do Servidor
		if(info == 0){
		read(fd, buffer, BUFLEN);
		buffer[BUFLEN - 1] = '\0';
		printf("Resposta:\n%s\n", buffer);
		}
			
	}
}


















