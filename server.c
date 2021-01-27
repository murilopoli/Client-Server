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

struct Quadro{    //Declaração da estrutura
	long int ID;
	long int tamanho;
	char data[BUFFERMAX];
};


int scan(FILE *f){    //Função para explorar arquivos que estão no servidor
	struct dirent **ponteiro; 
	int n = 0;
	if ((n = scandir(".", &ponteiro, NULL, alphasort)) < 0) {
		perror("Erro na busca");    //O programa não achou nenhum item
		return -1; 
	}
	while (n--) {
		fprintf(f, "%s\n", ponteiro[n]->d_name);    //Lista de arquivos disponíveis
		free(ponteiro[n]); 
	}
	free(ponteiro); 
	return 0; 
}

static void erro_msg(const char *msg, ...){    //Função para imprimir erros ocorridos
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv){    //Função main
	if ((argc < 2) || (argc > 2)) {
		printf("Erro nos parâmetros inseridos");    //Erro encontrado nos parâmetros de execução
		exit(EXIT_FAILURE);
	}
	ssize_t leitura;    //Variavel para guardar o tamanho da mensagem lida
	ssize_t tamanho;    //Variavel para guardar o tamanho da estrutura
	off_t tam_arq;      //Variavel para guardar o tamanho do arquivo
	long int ACK = 0;   //ACK de reconhecimento
	int ACK_env = 0;    //ACK de envio
	int sck;            // Numero do socket para a conexão
	FILE *arq;          //Ponteiro para arquivo
	struct sockaddr_in end_sv, end_cl;    //Declaração de variáveis de endereço
	struct stat var;                      //Variavel para guardar informações do arquivo
	struct Quadro q;                      //Variavel de estrutura
	struct timeval temporizador = {0, 0}; //Variavel de temporizador setado para 0
	char mensagem[BUFFERMAX];
	char nomearq[20];
	char comando[10];
	memset(&end_sv, 0, sizeof(end_sv));        //Setando o buffer
	end_sv.sin_family = AF_INET;
	end_sv.sin_port = htons(atoi(argv[1]));    //Setando porta
	end_sv.sin_addr.s_addr = INADDR_ANY;       //Setando endereço
	if ((sck = socket(AF_INET, SOCK_DGRAM, 0)) == -1)    //Socket não encontrado
		erro_msg("Erro no socket\n");
	if (bind(sck, (struct sockaddr *) &end_sv, sizeof(end_sv)) == -1)    //Endereço não encontrado
		erro_msg("Erro na conexão\n");
	for(;;){
		printf("Aguardando conexões\n");
		memset(mensagem, 0, sizeof(mensagem));    //Setando buffers para receber informações
		memset(comando, 0, sizeof(comando));
		memset(nomearq, 0, sizeof(nomearq));
		tamanho = sizeof(end_cl);
		if((leitura = recvfrom(sck, mensagem, BUFFERMAX, 0, (struct sockaddr *) &end_cl, (socklen_t *) &tamanho)) == -1)    //Erro ao receber a mensagem
			erro_msg("Erro de comunicação");
		printf("Mensagem recebida: %s\n", mensagem);
		sscanf(mensagem, "%s %s", comando, nomearq);
		if ((strcmp(comando, "Download") == 0) && (nomearq[0] != '\0')){    //Comando download recebido
			printf("Nome do arquivo requisitado: %s\n", nomearq);
			if (access(nomearq, F_OK) == 0) {    //Verificando arquivo
				int qtd_quadro = 0, qtd_reenviado = 0, qtd_perdido = 0, temporizador_flag = 0;
				long int i = 0;                  //Variaveis auxiliares
				stat(nomearq, &var);
				tam_arq = var.st_size;           //Tamanho do arquivo
				temporizador.tv_sec = 2;
				temporizador.tv_usec = 0;
				setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval));    //Definição do temporizador de recebimento
				arq = fopen(nomearq, "rb");
				if ((tam_arq % BUFFERMAX) != 0)
					qtd_quadro = (tam_arq / BUFFERMAX) + 1;    //Numero total de quadros a serem enviados
				else
					qtd_quadro = (tam_arq / BUFFERMAX);
				printf("Total de quadros: %d\n", qtd_quadro);
				tamanho = sizeof(end_cl);
				sendto(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));    //Numero dos quadros a serem enviados
				recvfrom(sck, &(ACK), sizeof(ACK), 0, (struct sockaddr *) &end_cl, (socklen_t *) &tamanho);
				while (ACK != qtd_quadro){    //Verificando variável de reconhecimento
					sendto(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_cl, sizeof(end_cl)); //Tentando a retransmissão
					recvfrom(sck, &(ACK), sizeof(ACK), 0, (struct sockaddr *) &end_cl, (socklen_t *) &tamanho);
					qtd_reenviado++;
					if (qtd_reenviado == 10) {    //Setando o flag do temporizador após 10 tentativas
						temporizador_flag = 1;
						break;
					}
				}
				for (i = 1; i <= qtd_quadro; i++){    //Transmitindo sequencialmente
					memset(&q, 0, sizeof(q));    //Setando buffer
					ACK = 0;
					q.ID = i;
					q.tamanho = fread(q.data, 1, BUFFERMAX, arq);
					sendto(sck, &(q), sizeof(q), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));    //Enviando quadro
					recvfrom(sck, &(ACK), sizeof(ACK), 0, (struct sockaddr *) &end_cl, (socklen_t *) &tamanho);  //Recebendo ACK
					while (ACK != q.ID){  //Verificando ACK, se houver problemas o programa retransmite
						sendto(sck, &(q), sizeof(q), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));
						recvfrom(sck, &(ACK), sizeof(ACK), 0, (struct sockaddr *) &end_cl, (socklen_t *) &tamanho);
						printf("Quadro %ld perdido %d vezes\n", q.ID, ++qtd_perdido);
						qtd_reenviado++;
						printf("Quadro %ld perdido %d vezes\n", q.ID, qtd_perdido);
						if (qtd_reenviado == 50){    //Setando o flag do temporizador após 50 tentativas
							temporizador_flag = 1;
							break;
						}
					}
					qtd_reenviado = 0;
					qtd_perdido = 0;
					if (temporizador_flag == 1){    //Erro de temporizador
						printf("Arquivo não enviado\n");
						break;
					}
					printf("Quadro %ld enviado com sucesso\n", i);
					if (qtd_quadro == ACK)
						printf("Arquivo enviado\n");
				}
				fclose(arq);
				temporizador.tv_sec = 0;
				temporizador.tv_usec = 0;
				setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval));
			}
			else {
				printf("Arquivo não encontrado\n");
			}
		}
		else if ((strcmp(comando, "Upload") == 0) && (nomearq[0] != '\0')){    //Comando upload recebido
			printf("Nome do arquivo: %s\n", nomearq);
			long int qtd_quadro = 0, recebido = 0, i = 0;    //Variveis auxiliares
			temporizador.tv_sec = 2;
			setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval));    //Setando temporizador caso não obtenha resposta
			recvfrom(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_cl, (socklen_t *) &tamanho);  //Numero de quadros a serem enviados
			temporizador.tv_sec = 0;
			setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (char *)&temporizador, sizeof(struct timeval));    //Desabilitando temporizador
			if (qtd_quadro > 0){
				sendto(sck, &(qtd_quadro), sizeof(qtd_quadro), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));
				printf("Total de Quadros: %ld\n", qtd_quadro);
				arq = fopen(nomearq, "wb");
				for (i = 1; i <= qtd_quadro; i++){    //Recebendo quadros e enviando ACKs
					memset(&q, 0, sizeof(q));
					recvfrom(sck, &(q), sizeof(q), 0, (struct sockaddr *) &end_cl, (socklen_t *) &tamanho);
				       	sendto(sck, &(q.ID), sizeof(q.ID), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));
					if ((q.ID < i) || (q.ID > i)){  //Removendo quadros repetidos
						i--;
					}
					else {
						fwrite(q.data, 1, q.tamanho, arq);    //Escrevendo arquivo
						printf("quadro %ld recebido\n", q.ID);
						recebido += q.tamanho;   
					}
					if (i == qtd_quadro)  //Verificação do total de quadros
						printf("Arquivo recebido\n");
				}
			       printf("Tamanho total:%ld\n", recebido);
			       fclose(arq);
			}
			else {
				printf("O arquivo está vazio\n");
			}
		}
		else if ((strcmp(comando, "Excluir") == 0) && (nomearq[0] != '\0')){    //Comando excluir selecionado
			if(access(nomearq, F_OK) == -1){    //Verificando arquivo
				ACK_env = -1;
				sendto(sck, &(ACK_env), sizeof(ACK_env), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));
			}
			else{
				if(access(nomearq, R_OK) == -1){    //Verificando permissão
					ACK_env = 0;
					sendto(sck, &(ACK_env), sizeof(ACK_env), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));
				}
				else {
					printf("Nome do arquivo: %s\n", nomearq);
					remove(nomearq);    //Removendo arquivo
					ACK_env = 1;
					sendto(sck, &(ACK_env), sizeof(ACK_env), 0, (struct sockaddr *) &end_cl, sizeof(end_cl));    //Enviando ACK de reconhecimento
				}
			}
		}
		else if(strcmp(comando, "Explorar") == 0){    //Comando explorar recebido
			char entrada[200];
			memset(entrada, 0, sizeof(entrada));       //Setando buffer
			arq = fopen("lista.log", "wb");            //Criando lista de arquivos
			if (scan(arq) == -1)                       //Nenhum arquivo encontrado
				erro_msg("Erro na procura");
			fclose(arq);
			arq = fopen("lista.log", "rb");	
			int listatam = fread(entrada, 1, 200, arq);//Lendo a lista de arquivos
			printf("Tamanho = %d\n", listatam);
			if (sendto(sck, entrada, listatam, 0, (struct sockaddr *) &end_cl, sizeof(end_cl)) == -1)    //Enviando a lista de arquivos
				erro_msg("Erro de envio");
			remove("lista.log");
			fclose(arq);
		}
		else if (strcmp(comando, "Sair") == 0){    //Comando sair recebido
			close(sck);    //Finalizando sessao
			exit(EXIT_SUCCESS);
		}
		else {
			printf("Comando desconhecido\n");      //Comando recebido desconhecido
		}
	}
	close(sck);
	exit(EXIT_SUCCESS);
}
