#include <infiniband/verbs.h>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <byteswap.h>

#define MAX_POLL_CQ_TIMEOUT 2000

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

using namespace std;

struct IBSYNMsg {
  uint64_t addr;
  uint32_t rkey;
  uint32_t qpn;
  uint16_t lid;
  uint8_t gid[16];
} __attribute__((packed));

class Infiniband{
public:
  ~Infiniband();
  void init();
  int create_qp();
  int post_work_request(int opcode);
  int post_recv();
  int poll_cq();
  void display();
  int qp_activate();
  int send_msg(int sock, int size, char *my_msg, char *peer_msg);
  int destroy();
public:
  bool is_init = false;
  int num_devices;
  int port_num;
  int cq_depth;
  int buffer_size;
  char *buff = nullptr;

  struct ibv_device **dev_list = nullptr;
  struct ibv_device *device = nullptr;
  struct ibv_context *ctxt = nullptr;
  struct ibv_port_attr port_attr;
  union ibv_gid gid;
  struct ibv_pd *pd = nullptr;
  struct ibv_cq *cq = nullptr;
  struct ibv_mr *mr = nullptr;
  struct ibv_qp *qp = nullptr;

  struct IBSYNMsg peer_msg;
  struct IBSYNMsg my_msg;
};
