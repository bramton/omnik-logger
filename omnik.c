#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <netdb.h> //getaddrinfo
#include <netinet/in.h> // sockaddr_in
//#include <sys/types.h>
#include <sys/socket.h>

#define BACKLOG 5
#define PORT 10004
#define PACKET_SIZE 170

#define HTTP_MSG_SIZE 512
#define HTTP_BODY_SIZE 512


struct inverter_msg {
  char sn[16+1];
  char fw_main[15+1];
  char fw_child[9+1];
  float temp;
  float v_pv[3];
  float i_pv[3];
  float i_ac[3];
  float v_ac[3];
  float f_ac[3];
  unsigned short p_ac[3];
  float e_today;
  float e_total;
};

// Source for message format:
// https://github.com/XtheOne/Inverter-Data-Logger/blob/master/InverterMsg.py
void unpack_msg(unsigned char *rmsg, struct inverter_msg *imsg) {
  // Bytes 15 - 30, serial number
  memcpy(imsg->sn, &rmsg[15], 17);
  imsg->sn[16] = '\0';

  // Bytes 31 - 32, temperature
  imsg->temp = rmsg[31] << 8 | rmsg[32];
  imsg->temp /= 10;

  for (int i = 0; i < 3; i++) {
    // Bytes 33 - 38, voltage pv
    imsg->v_pv[i] = rmsg[33 + i*2] << 8 | rmsg[34 + i*2];
    imsg->v_pv[i] /= 10;

    // Bytes 39 - 44, current pv
    imsg->i_pv[i] = rmsg[39 + i*2] << 8 | rmsg[40 + i*2];
    imsg->i_pv[i] /= 10;

    // Bytes 45 - 50, current ac
    imsg->i_ac[i] = rmsg[45 + i*2] << 8 | rmsg[46 + i*2];
    imsg->i_ac[i] /= 10;

    // Bytes 51 - 56, voltage ac
    imsg->v_ac[i] = rmsg[51 + i*2] << 8 | rmsg[52 + i*2];
    imsg->v_ac[i] /= 10;

    // Bytes 57 - 68, frequency ac, power ac, interleaved
    imsg->f_ac[i] = rmsg[57 + i*4] << 8 | rmsg[58 + i*4];
    imsg->f_ac[i] /= 100;
    imsg->p_ac[i] = rmsg[59 + i*4] << 8 | rmsg[60 + i*4];
  }

  // Bytes 69 - 70, total energy today
  imsg->e_today = rmsg[69] << 8 | rmsg[70];
  imsg->e_today /= 100;

  // Bytes 71 - 74, total energy all-time
  imsg->e_total = rmsg[71] << 24 | rmsg[72] << 16 | rmsg[73] << 8 | rmsg[74];
  imsg->e_total /= 10;
}

void dump_msg(struct inverter_msg *imsg) {
  printf("Serial number: %s\n", imsg->sn);
  printf("Temp: %.1f\n", imsg->temp);
  printf("E total: %.1f\n", imsg->e_total);
  printf("E today: %.1f\n", imsg->e_today);
  printf("V pv: %.1f\n", imsg->v_pv[0]);
  printf("I pv: %.1f\n", imsg->i_pv[0]);
  printf("V ac: %.1f\n", imsg->v_ac[0]);
  printf("I ac: %.1f\n", imsg->i_ac[0]);
  printf("f ac: %.1f Hz\n", imsg->f_ac[0]);
  printf("p ac: %u\n", imsg->p_ac[0]);
}

// This part has hard coded values. Should change.
// See: https://docs.victoriametrics.com/url-examples/#influxwrite
void http_request(struct inverter_msg *imsg) {
  char http_msg[HTTP_MSG_SIZE];
  char http_body[HTTP_BODY_SIZE];
  int offset;

  char host[] = "stats.lan.cbbg.nl";
  char path[] = "/write";
  unsigned int port = 8428;

  sprintf(http_body,"pv temp=%.2f,p=%u,f=%.2f,v_ac=%.2f,i_ac=%.2f,v_pv=%.2f,i_pv=%.2f",
    imsg->temp, imsg->p_ac[0], imsg->f_ac[0], imsg->v_ac[0], imsg->i_ac[0], imsg->v_pv[0], imsg->i_pv[0]);

  offset = sprintf(http_msg, "POST %s HTTP/1.1\r\n", path);
  offset += sprintf(http_msg+offset, "Host: %s:%d\r\n", host, port);
  offset += sprintf(http_msg+offset, "Connection: close\r\n");
  offset += sprintf(http_msg+offset, "Content-Length: %lu\r\n", strlen(http_body));
  offset += sprintf(http_msg+offset, "\r\n");
  offset += sprintf(http_msg+offset, "%s", http_body);
  printf("%s\n",http_msg);

  int sd;

  struct addrinfo hints, *res, *res0;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;


  int err = getaddrinfo(host, "8428", &hints, &res0);
  if (err) {
    perror("Error resolving hostname");
    exit(EXIT_FAILURE);
  }

  for (res =  res0; res; res = res->ai_next) {
    sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd < 0) {
      continue;
    }

    if (connect(sd, res->ai_addr, res->ai_addrlen) < 0) {
      sd = -1;
      continue;
    }

    break; /* Got succesful connection */
  }

  if (sd < 0) {
    perror("Failed socket() or connect()");
    exit(EXIT_FAILURE);
  }
  freeaddrinfo(res0);

  int nbytes = write(sd, http_msg, strlen(http_msg));
  if (nbytes < 0) {
    perror("Uh oh, writing problem");
  }

  close(sd);
}

int main() {
  int sd, sd_new; // Socket descriptors
  int len;
  unsigned char msg[PACKET_SIZE] = { 0 };
  struct sockaddr_in addr;
  struct inverter_msg imsg;

  memset(&addr, 0, sizeof(addr)); // Zero the addr befor populating

  sd = socket(PF_INET, SOCK_STREAM, 0);
  if (sd < 0) {
    perror("Error creating socket");
    exit(EXIT_FAILURE);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // see inet_aton, ip addres to dec
  addr.sin_port = htons(PORT);

  if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("Error binding");
    exit(EXIT_FAILURE);
  }

  if (listen(sd, BACKLOG) < 0) {
    perror("Error listening");
    exit(EXIT_FAILURE);
  }

  for (;;){
    sd_new = accept(sd, NULL, NULL);
    if (sd_new < 0) {
      perror("Error accepting");
      exit(EXIT_FAILURE);
    }

    len = recv(sd_new, msg, PACKET_SIZE, MSG_WAITALL);
    if (len < 0) {
      perror("Error reading");
      exit(EXIT_FAILURE);
    }

    if (len != PACKET_SIZE) {
      fprintf(stderr, "Packet size mismatch. Expected %d bytes, got %d bytes\n",
          PACKET_SIZE, len);
      continue;
    }

    //printf("Read %d bytes\n", len);
    unpack_msg(msg, &imsg);
    dump_msg(&imsg);
    //printf("%u %.1f %.1f\n", imsg.p_ac[0], imsg.f_ac[0], imsg.temp);
    //https://stackoverflow.com/questions/1716296/why-does-printf-not-flush-after-the-call-unless-a-newline-is-in-the-format-strin
    //fflush(stdout);
    close(sd_new);

    http_request(&imsg);
  }

  return 0;
}
