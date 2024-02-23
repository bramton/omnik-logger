#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <netinet/in.h> // sockaddr_in
//#include <sys/types.h>
#include <sys/socket.h>

#define BACKLOG 5

int main() {
  printf("Hallo\n");
  int sd, sd_new; // Socket descriptors
  int len;
  char msg[1024] = { 0 };
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(addr)); // Zero the addr befor populating

  sd = socket(PF_INET, SOCK_STREAM, 0);
  if (sd < 0) {
    perror("Error creating socket");
    exit(EXIT_FAILURE);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // see inet_aton, ip addres to dec
  addr.sin_port = htons(10004);

  if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("Error binding");
    exit(EXIT_FAILURE);
  }

  if (listen(sd, BACKLOG) < 0) {
    perror("Error listening");
    exit(EXIT_FAILURE);
  }

  sd_new = accept(sd, NULL, NULL);
  if (sd_new < 0) {
    perror("Error accepting");
    exit(EXIT_FAILURE);
  }
  printf("Connection accpeted");

  len = read(sd_new, msg, sizeof(msg));
  if (len < 0) {
    perror("Error reading");
    exit(EXIT_FAILURE);
  }

  printf("Read %d bytes\n", len);

  return 0;
}
