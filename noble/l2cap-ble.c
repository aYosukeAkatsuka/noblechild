#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define HCI_DEVICE_ID 0

#define ATT_CID 4

#define BDADDR_LE_PUBLIC       0x01
#define BDADDR_LE_RANDOM       0x02

struct sockaddr_l2 {
  sa_family_t    l2_family;
  unsigned short l2_psm;
  bdaddr_t       l2_bdaddr;
  unsigned short l2_cid;
  uint8_t        l2_bdaddr_type;
};

#define L2CAP_CONNINFO  0x02
struct l2cap_conninfo {
  uint16_t       hci_handle;
  uint8_t        dev_class[3];
};

int lastSignal = 0;

static void signalHandler(int signal) {
  fprintf(stderr, "[%s] !! l2cap SIGNAL Detected. signal = %d\n", __func__, signal);
  lastSignal = signal;
}

int main(int argc, const char* argv[]) {
  int hciSocket;

  int l2capSock;
  struct sockaddr_l2 sockAddr;
  struct l2cap_conninfo l2capConnInfo;
  socklen_t l2capConnInfoLen;
  int hciHandle;
  int result;

  fd_set rfds;
  struct timeval tv;

  char stdinBuf[256 * 2 + 1];
  char l2capSockBuf[256];
  int len;
  int i;

  setvbuf(stdout, NULL, _IONBF, 0);

  // setup signal handlers
  signal(SIGINT, signalHandler);
  signal(SIGKILL, signalHandler);
  signal(SIGHUP, signalHandler);
  signal(SIGUSR1, signalHandler);

  prctl(PR_SET_PDEATHSIG, SIGINT);

  hciSocket = hci_open_dev(HCI_DEVICE_ID);

  // create socket
  l2capSock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

  // bind
  memset(&sockAddr, 0, sizeof(sockAddr));
  sockAddr.l2_family = AF_BLUETOOTH;
  bacpy(&sockAddr.l2_bdaddr, BDADDR_ANY);
  sockAddr.l2_cid = htobs(ATT_CID);

  result = bind(l2capSock, (struct sockaddr*)&sockAddr, sizeof(sockAddr));

  fprintf(stderr, "bind %s\n", (result == -1) ? strerror(errno) : "success");

  // connect
  memset(&sockAddr, 0, sizeof(sockAddr));
  sockAddr.l2_family = AF_BLUETOOTH;
  str2ba(argv[1], &sockAddr.l2_bdaddr);
  sockAddr.l2_bdaddr_type = strcmp(argv[2], "random") == 0 ? BDADDR_LE_RANDOM : BDADDR_LE_PUBLIC;
  sockAddr.l2_cid = htobs(ATT_CID);

  result = connect(l2capSock, (struct sockaddr *)&sockAddr, sizeof(sockAddr));

  l2capConnInfoLen = sizeof(l2capConnInfo);
  getsockopt(l2capSock, SOL_L2CAP, L2CAP_CONNINFO, &l2capConnInfo, &l2capConnInfoLen);
  hciHandle = l2capConnInfo.hci_handle;

  printf("connect %s\n", (result == -1) ? strerror(errno) : "success");

  while(1) {
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    FD_SET(l2capSock, &rfds);

    tv.tv_sec = 1;
    tv.tv_usec = 0;

	fprintf(stderr, "beginning of while loop\n");
    result = select(l2capSock + 1, &rfds, NULL, NULL, &tv);

	fprintf(stderr, "select result = %d, lastSignal = %d\n", result, lastSignal);
    if (-1 == result) {
      if (SIGINT == lastSignal || SIGKILL == lastSignal || SIGHUP == lastSignal) {
		fprintf(stderr, "signal to termination received.\n");
        break;
      }

      if (SIGUSR1 == lastSignal) {
        int8_t rssi = 0;

        for (i = 0; i < 100; i++) {
          hci_read_rssi(hciSocket, hciHandle, &rssi, 1000);

          if (rssi != 0) {
            break;
          }
        }
        
        if (rssi == 0) {
          rssi = 127;
        }

        printf("rssi = %d\n", rssi);
      }
    } else if (result) {
		int j, n;
      if (FD_ISSET(0, &rfds)) {
        len = read(0, stdinBuf, sizeof(stdinBuf));
		fprintf(stderr, "read len = %d\n", len);
		fprintf(stderr, "stdinBuf =\n");
		for (j = 0; j < len; j++) {
			fprintf(stderr, "%c", stdinBuf[j]);
		}
		fprintf(stderr, "\n");

        i = 0;
        while(stdinBuf[i] != '\n') {
          sscanf(&stdinBuf[i], "%02x", (unsigned int*)&l2capSockBuf[i / 2]);

          i += 2;
        }

        n = write(l2capSock, l2capSockBuf, (len - 1) / 2);
		fprintf(stderr, "n = %d\n", n);
		fprintf(stderr, "l2capSockBuf =\n");
		for (j = 0; j < (len - 1) / 2; j++) {
			fprintf(stderr, "%02x ", l2capSockBuf[j]);
		}
		fprintf(stderr, "\n");

      }

      if (FD_ISSET(l2capSock, &rfds)) {
        len = read(l2capSock, l2capSockBuf, sizeof(l2capSockBuf));

        if (len <= 0) {
          break;
        }

        printf("data ");
        for(i = 0; i < len; i++) {
          printf("%02x", ((int)l2capSockBuf[i]) & 0xff);
        }
        printf("\n");
      }
    }
  }

  close(l2capSock);
  close(hciSocket);
  printf("disconnect\n");
  fprintf(stderr, "disconnect\n");

  return 0;
}
