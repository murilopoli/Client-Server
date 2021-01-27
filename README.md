****************************************************
The ZIP file has two files: cliente.c and server.c

The files were implemented in Ubuntu 12.04 LTS

In them, reliable data transfer was implemented through variables and checks, they are:

Sequence number

Recognition package

Timer
****************************************************
To run the server:

Compile in the terminal: gcc server.c -o server

Run in the terminal: ./server PORTA

The port must be greater than 5000
****************************************************
To run the client:

Compile in the terminal: gcc cliente.c -o cliente

Run in the terminal: ./cliente PORT_IP Address

The port must be the same as the port entered when running the server
****************************************************
Libraries used:
netdb.h
unistd.h
fcntl.h
errno.h
string.h
stdarg.h
dirent.h
sys/types.h
sys/socket.h
sys/stat.h
netinet/in.h
arpa/inet.h

====================================================
O arquivo ZIP possui dois arquivos: cliente.c e server.c
Os arquivos foram implementados no Ubuntu 12.04 LTS
Neles foram implementados a transferência confiável de dados por meio de variáveis e verificações, são elas:
Numero de sequência
Pacote de reconhecimento
Temporizador
****************************************************
Para executar o server:
Compilar no terminal: gcc server.c -o server
Executar no terminal: ./server PORTA
A porta deve ser maior que 5000
****************************************************
Para executar o cliente:
Compilar no terminal: gcc cliente.c -o cliente
Executar no terminal: ./cliente ENDEREÇO_IP PORTA
A porta deve ser igual a porta inserida na execução do server
****************************************************
Bibliotecas utilizadas:
netdb.h
unistd.h
fcntl.h
errno.h
string.h
stdarg.h
dirent.h
sys/types.h
sys/socket.h
sys/stat.h
netinet/in.h
arpa/inet.h

