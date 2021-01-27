#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>

#define BUFFERMAX (2048)    //Tamanho do buffer

struct Quadro {    //Declaração da estrutura
	long int ID;
	long int tamanho;
	char data[BUFFERMAX];
};

static void erro_msg(char *msg){    //Função para imprimir erros ocorridos
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv){    //Função main
	if ((argc < 3) || (argc >3)){
		printf("Erro nos parâmetros inseridos\n");    //Erro encontrado nos parâmetros de execução
		exit(EXIT_FAILURE);
	}
	ssize_t leitura = 0;    //Variavel para guardar o tamanho da mensagem lida
	ssize_t tamanho = 0;    //Variavel para guardar o tamanho da estrutura
	off_t tam_arq = 0;      //Variavel para guardar o tamanho do arquivo
	long int ACK = 0;       //ACK de reconhecimento
	int sck, ack_recebido = 0;  //Variavel que acessa socket e variavel que armazena o ack recebido
	FILE *arq;
	struct sockaddr_in end_enviar, end_receber;    //Declaração de variáveis de endereço
	struct stat var;                               //Variavel para guardar informações do arquivo
	struct Quadro q;                               //Variavel de estrutura
	struct timeval temporizador = {0, 0};    //Variavel de temporizador setado para 0
	char cmd_enviado[50];
	char nomearq[20];
	char cmd[10];
	char ack_env[4] = "ACK";
	memset(ack_env, 0, sizeof(ack_env));           //Setando o buffer
	memset(&end_enviar, 0, sizeof(end_enviar));
	memset(&end_receber, 0, sizeof(end_receber));
	end_enviar.sin_family = AF_INET;
	end_enviar.sin_port = htons(atoi(argv[2]));            //Setando porta
	end_enviar.sin_addr.s_addr = inet_addr(argv[1]);       //Setando endereço
	if ((sck = socket(AF_INET, SOCK_DGRAM, 0)) == -1)      //Socket não encontrado
		erro_msg("Erro no socket");
	for (;;) {
		memset(cmd_enviado, 0, sizeof(cmd_enviado));       //Setando o buffer
		memset(cmd, 0, sizeof(cmd));
		memset(nomearq, 0, sizeof(nomearq));
		printf("\n Menu \n Entre com a opção desejada\n 1)Download nomearq\n 2)Upload nomearq\n 3)Excluir nomearq\n 4)Explorar\n 5)Sair\n\n");
		scanf(" %[^\n]%*c", cmd_enviado);
		sscanf(cmd_enviado, "%s %s", cmd, nomearq);		//Recebe o comando e nome do arquivo
		if (sendto(sck, cmd_enviado, sizeof(cmd_enviado), 0, (struct sockaddr *) &end_enviar, sizeof(end_enviar)) == -1) {
			erro_msg("Erro de comunicação com o servidor");
		}
		if ((strcmp(cmd, "Download") == 0) && (nomearq[0] != '\0' )){    //Comando download recebido
			long int qtd_quadro = 0;                                     //Quantidade de quadros
			long int qtd_bytes = 0, i = 0;                               //Quantidade de bytes
			temporizador.tv_sec = 2;
			setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval));    //Setando temporizador caso nao obtenha resposta
			recvfrom(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_receber, (socklen_t *) &tamanho); //Recebe o total de quadros a receber
			temporizador.tv_sec = 0;
			setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval));               //Desabilitando temporizador
			if (qtd_quadro > 0) {
				sendto(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_enviar, sizeof(end_enviar));    //Enviando total de quadros
				arq = fopen(nomearq, "wb");	   //Abrindo o arquivo para escrever
				for (i = 1; i <= qtd_quadro; i++){    //Transmitindo sequencialmente
					memset(&q, 0, sizeof(q));
					recvfrom(sck, &(q), sizeof(q), 0, (struct sockaddr *) &end_receber, (socklen_t *) &tamanho);    //Recebendo quadro
					sendto(sck, &(q.ID), sizeof(q.ID), 0, (struct sockaddr *) &end_enviar, sizeof(end_enviar));     //Enviando ACK
					if ((q.ID < i) || (q.ID > i))    //Eliminar quadros duplicados
						i--;
					else {
						fwrite(q.data, 1, q.tamanho, arq);    //Escrevendo no arquivo
						printf("Quadro:%ld\n", q.ID);
						qtd_bytes += q.tamanho;
					}
					if (i == qtd_quadro){    //Verificando o total de quadros
						printf("Arquivo recebido\n");
					}
				}
				printf("Total de bytes recebidos:%ld\n", qtd_bytes);
				fclose(arq);
			}
			else{
				printf("O arquivo está vazio\n");
			}
		}
		else if ((strcmp(cmd, "Upload") == 0) && (nomearq[0] != '\0')) {    //Comando upload recebido
			if (access(nomearq, F_OK) == 0) {    //Verificando se o arquivo existe
				int qtd_quadro = 0, qtd_reenviado = 0, qtd_perdido = 0, temporizador_flag = 0;  //Variaveis auxiliares
				long int i = 0;
				stat(nomearq, &var);
				tam_arq = var.st_size;    //Armazenando o tamanho do arquivo
				temporizador.tv_sec = 2;
				temporizador.tv_usec = 0;
				setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval)); //Setando temporizador para enviar
				arq = fopen(nomearq, "rb");    //Abrindo arquivo para enviar
				if ((tam_arq % BUFFERMAX) != 0)
					qtd_quadro = (tam_arq / BUFFERMAX) + 1;    //Numero total de quadros a ser enviado
				else
					qtd_quadro = (tam_arq / BUFFERMAX);
				printf("Numero total de pacotes %d", qtd_quadro);
				sendto(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_enviar, sizeof(end_enviar));    //Enviando o total de quadros
				recvfrom(sck, &(ACK), sizeof(ACK), 0, (struct sockaddr *) &end_receber, (socklen_t *) &tamanho);    //Recebendo ACK 
				while (ACK != qtd_quadro){    //Verificando variavel de reconhecimento
					sendto(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_enviar, sizeof(end_enviar));    //Tentando a retransmissão
					recvfrom(sck, &(ACK), sizeof(ACK), 0, (struct sockaddr *) &end_receber, (socklen_t *) &tamanho);           //Recebendo ACK
					qtd_reenviado++;
					if (qtd_reenviado == 10){    //Setando o flag do temporizador após 10 tentativas
						temporizador_flag = 1;
						break;
					}
				}
				for (i = 1; i <= qtd_quadro; i++){    //Transmitindo sequencialmente
					memset(&q, 0, sizeof(q));         //Setando buffer
					ACK = 0;
					q.ID = i;
					q.tamanho = fread(q.data, 1, BUFFERMAX, arq);
					sendto(sck, &(q), sizeof(q), 0, (struct sockaddr *) &end_enviar, sizeof(end_enviar));    //Enviando quadro
					recvfrom(sck, &(ACK), sizeof(ACK), 1, (struct sockaddr *) &end_receber, (socklen_t *) &tamanho);  //Recebendo ACK
					while (ACK != q.ID){      //Verificando variavel de reconhecimento
						sendto(sck, &(q), sizeof(q), 0, (struct sockaddr *) &end_enviar, sizeof(end_enviar));
						recvfrom(sck, &(ACK), sizeof(ACK), 0, (struct sockaddr *) &end_receber, (socklen_t *) &tamanho);
						printf("Quadro %ld perdido %d vezes\n", q.ID, ++qtd_perdido);
						qtd_reenviado++;
						if (qtd_reenviado == 50){    //Setando o flag do temporizador após 50 tentativas
							temporizador_flag = 1;
							break;
						}
					}
					qtd_perdido = 0;
					qtd_reenviado = 0;
					if (temporizador_flag == 1){    //Erro de temporizador
						printf("O arquivo não foi enviado\n");
						break;
					}
					printf("Pacote %ld enviado com sucesso\n", i);
					if (qtd_quadro == ACK)
						printf("Arquivo enviadot\n");
				}
				fclose(arq);
				temporizador.tv_sec = 0;
				setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval));    //Desabilitando temporizador
			}
		}
		else if ((strcmp(cmd, "Excluir") == 0) && (nomearq[0] != '\0')){    //Comando excluir selecionado
			tamanho = sizeof(end_receber);
			ack_recebido = 0;
			if((leitura = recvfrom(sck, &(ack_recebido), sizeof(ack_recebido), 0,  (struct sockaddr *) &end_receber, (socklen_t *) &tamanho)) < 0)    //Recebendo ACK
				erro_msg("Erro de comunicanicação");
			if (ack_recebido > 0)
				printf("Arquivo excluido\n");
			else if (ack_recebido < 0)    //Arquivo não encontrado
				printf("Arquivo não encontrado\n");
			else
				printf("O arquivo não tem permissões para ser enviado\n");
		}
		else if (strcmp(cmd, "Explorar") == 0){    //Comando explorar selecionado
			char nomearq[200];
			memset(nomearq, 0, sizeof(nomearq)); //Setando buffer
			tamanho = sizeof(end_receber);
			if ((leitura = recvfrom(sck, nomearq, sizeof(nomearq), 0,  (struct sockaddr *) &end_receber, (socklen_t *) &tamanho)) < 0)    //Verificando nome do arquivo
				erro_msg("Erro de comunicação");
			if (nomearq[0] != '\0'){
				printf("\nLista de arquivos:\n%s \n", nomearq);
			}
			else {
				printf("Lista vazia");
				continue;
			}
		}
		else if (strcmp(cmd, "Sair") == 0){    //Comando sair recebido
			exit(EXIT_SUCCESS);
		}
		else{
			printf("Comando desconhecido\n");    //Comando recebido desconhecido
		}
	}
	close(sck);
	exit(EXIT_SUCCESS);
}
