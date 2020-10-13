// header files kept in one place
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

#define MAX_SIZE 512
// error display function
int sys_error(const char *x)
{
	perror(x);
	exit(1);
}
// time out function call
int timeout_call(int fd, int time){
	
	fd_set rcv_set;
	struct timeval t_val;
	FD_ZERO(&rcv_set);  // setting the recv 
	FD_SET (fd, &rcv_set);
	t_val.tv_sec = time;
	t_val.tv_usec = 0;
	
	return (select (fd + 1, &rcv_set, NULL, NULL, &t_val)); //timeout_call shall be called with in 1 sec of call 

	
}
// main function to handle client request
int main (int argc, char *argv[]){
	
	char input_dat_rd[512] = {0}; //populating with null value
	int socket_fd, new_socket_fd;
	struct sockaddr_in client;
	socklen_t client_ln = sizeof(struct sockaddr_in);
	
	int g, ret_add;
	int send_resp;
	int last_b;  // last beat response 
	
	int ret_value, rcv_byte;
	char buffer[1024] = {0};
	char ack[32] = {0};
	char payload[516] = {0};
	char fl_payload_copy[516] = {0};
	char fl_name[MAX_SIZE];
	char mode[512];  // mode value set 
	
	unsigned short int opcode1, opcode2, block_no;
	int b,j;//control handle variables for looping 
	
	FILE *fl_pointer;  // file pointer
// structure information with dynamic address and pointer variables	
	struct addrinfo dynamic_address, *addi, *cl_information, *p;
	int conf;
	int pid;
	int rd_ret;
	int block_num=0;
	char ip[INET6_ADDRSTRLEN];
	int timeout_call_c = 0; //time out call
	int count =0; //count check default value
	char c;
	char next_char = -1;
	int n_ackn = 0;  // initial value for negative acknowledgement 
	char *ep_port;
	
	ep_port = malloc (sizeof ep_port);
	
	if (argc!=3)
	{
		sys_error ("Correct Use: ./server <ip> <portnumber>");
		return 0;  // return call
	}
	
	conf = 1;
  //value for  addrlen;
  //address set 
	memset(&dynamic_address, 0, sizeof dynamic_address);
  dynamic_address.ai_family   = AF_INET;
  dynamic_address.ai_socktype = SOCK_DGRAM;
  dynamic_address.ai_flags    = AI_PASSIVE;
  
  // error handling in case of address check with dynamic address value 
   if ((ret_add = getaddrinfo(NULL, argv[2], &dynamic_address, &addi)) != 0) {
    fprintf(stderr, "Error: %s\n", gai_strerror(ret_add));
    exit(1);
  }
 // end of if condiiton  
    for(p = addi; p != NULL; p = p->ai_next) {
    socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (socket_fd < 0) {
      continue;//setup for socket not needed 
    }
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &conf, sizeof(int));
    if (bind(socket_fd, p->ai_addr, p->ai_addrlen) < 0) {
      close(socket_fd); // bind issues failures 
      continue;
    }
    break;// loop for exit it out
  }
  
  freeaddrinfo(addi);// deallocate the address value 
  printf(" Server Ready Now. Client connection wait state...\n");
 // default infinit loop for server  
  while(1){
	  //socket address rcv  
	  rcv_byte = recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client, &client_ln);
	  
	  //return condition if no bytes received  
	if(rcv_byte<0){
		sys_error("ERROR: no bytes received ");
		return 0;
	  }
	  // proceed if data is received 
	  
	memcpy (&opcode1, &buffer, 2);
	opcode1 = ntohs(opcode1);
	
	pid = fork();//child process creation  
	
	if(pid==0){//Child Process Active here 
		
		
		if (opcode1 ==1 ){// RRQ process start , do the process.
		 
			
			bzero(fl_name, MAX_SIZE);
			
			// checking for the EOF
			
			for (b=0; buffer[2+b]!='\0'; b++)
			{
				fl_name[b] = buffer[2+b];
			}
			
			fl_name[b] = '\0';
			bzero(mode, 512);
			g=0; // mode counter check 
			
			for (j = b+3; buffer[j] != '\0'; j++) {    // until \0 mode is held 
				mode[g]=buffer[j];
				g++;
			}	
			mode[g]='\0';// 
			printf("RRQ: File name: %s Mode: %s\n", fl_name, mode);
			
			// File handler. 
			
			fl_pointer = fopen(fl_name, "r");
			
			if (fl_pointer!=NULL){// file is not null case proceeded  
				
				close(socket_fd);// sending on the epephermal port
				*ep_port = htons(0);
				
				if ((ret_add = getaddrinfo(NULL, ep_port, &dynamic_address, &cl_information)) != 0) {
					fprintf(stderr, "ret_add: %s\n", gai_strerror(ret_add));
					return 1;
				}
				
				for(p = cl_information; p != NULL; p = p->ai_next) {
					if ((new_socket_fd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {// checking condition  
					sys_error("ERROR: SERVER (child): New socket N/A");
					continue;
					}
            
					setsockopt(new_socket_fd,SOL_SOCKET,SO_REUSEADDR,&conf,sizeof(int));
					if (bind(new_socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
						close(new_socket_fd);
						sys_error("ERROR: SERVER (new_socket_fd): issue with bind.");
						continue;
					}
					break;
				}	
				freeaddrinfo(cl_information);// 
				
				
				bzero(payload, sizeof(payload));//default zero value  
				bzero(input_dat_rd, sizeof(input_dat_rd));
				
				//read from the file 
				rd_ret = fread(&input_dat_rd,1,512,fl_pointer);
				
				if (rd_ret>=0){
					input_dat_rd[rd_ret] = '\0'; // for \0 case  
					printf("READ value is %d\n",rd_ret);
				}
				
				if(rd_ret < 512)
					last_b = 1;
				
				block_no = htons(1);                            
				opcode2 = htons(3);                              
				memcpy(&payload[0], &opcode2, 2);            
				memcpy(&payload[2], &block_no, 2);        
			
				
				memcpy(&payload[4], input_dat_rd, rd_ret); // make copy of the bytes 
				send_resp = sendto(new_socket_fd, payload, (rd_ret + 4), 0, (struct sockaddr*)&client, client_ln);
				
				n_ackn = 1;
				if (send_resp < 0)
					sys_error("ERROR in sending initial data packet: \n");
				else 
					printf("SENT: First block Success .\n");

				while(1){
					if(timeout_call(new_socket_fd,1)!= 0){
						bzero(buffer,sizeof(buffer));
						bzero(payload,sizeof(payload));
						rcv_byte = recvfrom(new_socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client,&client_ln);
						timeout_call_c = 0;
						if(rcv_byte < 0){
							sys_error("ERROR: Data Unavailable");
							return 6;
						}
						else{
							printf("RECEIVED DATA");
						}
						memcpy(&opcode1, &buffer[0],2);
						if(ntohs(opcode1)==4){					//OPCODE = 4; ACK
							bzero(&block_num, sizeof(block_num));
							memcpy(&block_num,&buffer[2],2);
							block_num = ntohs(block_num);
							printf("Ack %i received\n",block_num);
							if(block_num == n_ackn){
								printf("Expected Ack reached\n");
								n_ackn = (n_ackn + 1)%65536;
								if(last_b == 1){
									close(new_socket_fd);
									fclose(fl_pointer);
									printf("SERVER: File transfer complete \n");
									exit(5);
									last_b = 0;
								}
								else{
									bzero(input_dat_rd,sizeof(input_dat_rd));
									rd_ret = fread (&input_dat_rd,1,512,fl_pointer);
									if(rd_ret >= 0){
										if(rd_ret < 512)
											last_b = 1;
										input_dat_rd[rd_ret]='\0';
										block_no = htons(((block_num+1)%65536)); 
										opcode2 = htons(3);
										memcpy(&payload[0],&opcode2,2);
										memcpy(&payload[2],&block_no,2);
										printf("Bytes that are read from file %d",rd_ret);
										memcpy(&payload[4],input_dat_rd, rd_ret);
										int send_resp = sendto(new_socket_fd,payload,(rd_ret+4),0,(struct sockaddr*)&client, client_ln);

										if(send_resp < 0)
											sys_error("ERROR: SENDING");
									}
								}
							}
							else {
								printf("ERROR: Expected ACK has not reached :NEG ACK = %d, Block number =%d\n", n_ackn,block_no);
							}
						}
					}
					else{
						timeout_call_c ++ ;
						printf("TIMEOUT has happened\n");
						if(timeout_call_c == 10){
							printf("TIMEOUT has happened 10 times. Closing connection \n");
							close(new_socket_fd);
							fclose(fl_pointer);
							exit(6);
						}
						else{
							bzero(payload,sizeof(payload));
							memcpy(&payload[0],&fl_payload_copy[0],516);
							memcpy(&block_no,&payload[2],2);
							block_no = htons(block_no);
							printf("RETRANSMIT: Data with Block number : %d\n",block_no );
							send_resp = sendto(new_socket_fd,fl_payload_copy,(rd_ret+4),0,(struct sockaddr*)&client,client_ln);
							bzero(fl_payload_copy,sizeof(fl_payload_copy));
							memcpy(&fl_payload_copy[0],&payload[0],516);

							if(send_resp < 0)
								sys_error("ERROR: In sendto ");
							}		
						}
					}
				}
				else {
					unsigned short int E_CODE = htons(1);
					unsigned short int E_OPCODE = htons(5);
					char E_MSG[512] = "File not found";
					char ERROR_BUFFER [516] = {0};
					memcpy(&ERROR_BUFFER[0],&E_OPCODE,2);
					memcpy(&ERROR_BUFFER[2],&E_CODE,2);
					memcpy(&ERROR_BUFFER[4], &E_MSG, 512);
					sendto(socket_fd,ERROR_BUFFER, 516, 0 ,(struct sockaddr*)&client, client_ln);
					printf("SERVER CLEANUP: Filename Mismatch\n");
					close(socket_fd);
					fclose(fl_pointer);
					exit(4);
				}
			}
			else if(opcode1 == 2){
				*ep_port = htons(0);
				if((ret_add = getaddrinfo(NULL, ep_port, &dynamic_address, &cl_information)) != 0){
					fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(ret_add) );
					return 10;
				}
				for (p = cl_information; p != NULL; p = p->ai_next){
					if((new_socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))== -1){
						sys_error("ERROR: SERVER: child socket");
						continue;
					}
					setsockopt(new_socket_fd,SOL_SOCKET,SO_REUSEADDR,&conf, sizeof(int));
					if(bind(new_socket_fd, p->ai_addr, p->ai_addrlen) == -1){
						close(new_socket_fd);
						sys_error("ERROR: SERVER: bind new socket");
						continue;
					}
					break;
				}
				freeaddrinfo(cl_information);
				printf("WRQ: Client sending wrq to Server \n");
							bzero(fl_name, MAX_SIZE);
			
			
			for (b=0; buffer[2+b]!='\0'; b++)
			{
				fl_name[b] = buffer[2+b];
			}
			
			fl_name[b] = '\0';
				printf("FILENAME : %s\n",fl_name );
				FILE *fl_pointer_write = fopen(fl_name, "w+"); 
				if(fl_pointer_write == NULL)
				{
					printf("SERVER:WRQ: Unable to open File\n");
				}
				opcode2 = htons(4);
				block_no =  htons(0);
				bzero(ack, sizeof(ack));
				memcpy(&ack[0],&opcode2,2);
				memcpy(&ack[2],&block_no, 2);
				send_resp = sendto(new_socket_fd, ack, 4, 0, (struct sockaddr*)&client, client_ln);
				n_ackn = 1;
				if(send_resp < 0)
					sys_error("ERROR: WRQ: ACK: sendto");
				while(1){
					bzero(buffer, sizeof(buffer));
					rcv_byte = recvfrom(new_socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client, &client_ln);
					if(rcv_byte < 0){
						sys_error("ERROR:WRQ: Data not received");
						return 9;
					}
					bzero(input_dat_rd, sizeof(input_dat_rd));
					memcpy(&block_no,&buffer[2],2);
					g=rcv_byte;
					for(b =0; g>0;g--){
							input_dat_rd[b] = buffer[b+4];
							b++;
					}
					fwrite(input_dat_rd,1, (rcv_byte- 4),fl_pointer_write);
					block_no = ntohs(block_no);
// block condition 
					if(n_ackn == block_no){
						printf("SERVER: DATA RECEIVED: BLOCK NUMBER :%d\n",n_ackn );
						printf("SERVER: DATA EXPECTED received\n");
						opcode2 = htons(4);
						block_no = ntohs(n_ackn);
						bzero(ack,sizeof(ack));
						memcpy(&ack[0],&opcode2,2);
						memcpy(&ack[2],&block_no,2);
						printf("SERVER ACK SENT for block number %d\n",htons(block_no) );
						send_resp = sendto(new_socket_fd,ack,4,0,(struct sockaddr*)&client, client_ln);
						if(rcv_byte < 516){
							printf("data received .Closing  the connection now\n");
							close(new_socket_fd);
							fclose(fl_pointer_write);
							exit(9);

						}
						n_ackn = (n_ackn + 1)%65536;
					}
				}
			}
		
		}
	}
	
  }
  

