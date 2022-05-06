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


int send_msg(int sock, int size, char *my_msg, char *peer_msg)
{
  int rc;
  int read_bytes = 0;
  int total_read_bytes = 0;
  rc = ::write(sock, my_msg, size);
  if (rc < size)
    cout << "send msg failed" << endl;
  else {
    rc = 0;
    cout << "send msg done" << endl;
  }
  while (!rc && total_read_bytes < size)
  {
    read_bytes = ::read(sock, peer_msg, size);
    if(read_bytes > 0)
      total_read_bytes += read_bytes;
    else
      rc = read_bytes;
  }
  if(total_read_bytes == size)
    cout << "recv msg done" << endl;

  return rc;
}

int main(int argc, char *argv[])
{
  bool server = true;
  int listen_fd;
  int cfd;
  int sd;
  if(argc != 3) { 
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(6800);
    
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    ::bind(listen_fd, (struct sockaddr*)&sin, sizeof(sin));
    ::listen(listen_fd, 512);
    cout << "waiting client connection..." << endl;
    sd = ::accept(listen_fd, NULL, 0);
  } else {
    cfd = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *h;
    h = gethostbyname("0.0.0.0");

    struct sockaddr_in cin;
    memset(&cin, 0, sizeof(cin));
    cin.sin_family = AF_INET;
    cin.sin_port =  htons(6800);
    memcpy(&cin.sin_addr, h->h_addr, h->h_length);
    ::connect(cfd, (struct sockaddr*)&cin, sizeof(cin));
  }
  cout << "TCP established" << endl;

  int r = 0;
  struct ibv_device **dev_list = NULL;
  int num_devices;

  dev_list = ibv_get_device_list(&num_devices); //get rdma devices

  cout << "rdma dev count: " << num_devices << endl;

  const char* name;
  name = ibv_get_device_name(dev_list[0]);
  cout << "rdma dev name: " << name << endl;

  struct ibv_device *device;
  device = dev_list[0];

  struct ibv_context *ctxt;
  ctxt = ibv_open_device(device); // get rdma deives context

  if(ctxt == NULL) {
    cout << "open rdma deive context failed" << errno <<endl;
  }

//  ibv_free_device_list(dev_list);
//  dev_list = NULL;
//  device = NULL;

  int port_num = 1;
  struct ibv_port_attr port_attr;
  r = ibv_query_port(ctxt, port_num, &port_attr);
  if(r == -1) {
    cout << "query port failed" << endl;
  }

  union ibv_gid gid;
  r = ibv_query_gid(ctxt, port_num, 0, &gid);
  cout << "lid=" << port_attr.lid << endl;

  struct ibv_pd *pd;
  pd = ibv_alloc_pd(ctxt); // alloc protection domain
  if(pd == NULL) {
    cout << "alloc pd failed" << endl;
    r = -1;
  }

  struct ibv_cq *cq;
  int cq_depth = 1;
  cq =  ibv_create_cq(ctxt, cq_depth, NULL, NULL, 0); // get completion queue
  if(cq == NULL) {
    cout << "create cq failed" << endl;
    r = -1;
  }

  const char *user_write_buff = "send message";
  int msg_buff_size = strlen(user_write_buff) + 1;
  char *buff = (char*)malloc(msg_buff_size);
  
  memset(buff, 0, msg_buff_size);
  strcpy(buff, user_write_buff);
  cout << "ready to send_message: " << buff << endl;
  
  int mr_flag;
  struct ibv_mr *mr;
  mr = ibv_reg_mr(pd, buff, msg_buff_size, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ); // register memory
  if(mr == NULL) {
    cout << "rdma register memory failed" << endl;
    r = -1;
  }

  cout << "register memory successed, detail info: offset=" << &buff 
       << " length=" << msg_buff_size << " lkey=" 
       << mr->lkey << " rkey=" << mr->rkey << endl;

//  union ibv_gid cgid;

  struct ibv_qp_init_attr qpia;  // ibv_qp_init_attr use to descrip QP attr
  memset(&qpia, 0, sizeof(qpia));
  qpia.qp_type = IBV_QPT_RC; // this is a reliable connection
  qpia.sq_sig_all = 1;
  qpia.send_cq = cq;
  qpia.recv_cq = cq;
  qpia.cap.max_send_wr = 1;
  qpia.cap.max_recv_wr = 1;
  qpia.cap.max_send_sge = 1;
  qpia.cap.max_recv_sge = 1;

  struct ibv_qp *qp;
  qp =  ibv_create_qp(pd, &qpia);
  if(qp == NULL) {
    cout << "failed to create QP" << endl;
    r = -1;
  }
  cout << "create QP successed, QP number=" << qp->qp_num << endl;
 
  struct ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  qpa.qp_state = IBV_QPS_INIT;
  qpa.pkey_index = 0;
  qpa.port_num = 1;
  qpa.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;
  qpa.qkey = 0;
  int mask = IBV_QP_STATE | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS | IBV_QP_PKEY_INDEX;

  r = ibv_modify_qp(qp, &qpa, mask);
  if(r) {
    cout << "modify QP state to init failed" << endl;
    r = -1;
  }
  cout << "successed modify QP state to init" << endl;

  struct IBSYNMsg my_msg;
  struct IBSYNMsg peer_msg;
  struct IBSYNMsg recv_msg;

  my_msg.addr = htonll((uintptr_t)buff);
  my_msg.rkey = htonl(mr->rkey);
  my_msg.qpn = htonl(qp->qp_num);
  my_msg.lid = htons(port_attr.lid);  
  memcpy(my_msg.gid, &gid, 16);

  if(argc == 3)   
    send_msg(cfd, sizeof(struct IBSYNMsg), (char*)&my_msg, (char*)&recv_msg);
  else 
    send_msg(sd, sizeof(struct IBSYNMsg), (char*)&my_msg, (char*)&recv_msg);

  peer_msg.addr = ntohll(recv_msg.addr);
  peer_msg.rkey = ntohl(recv_msg.rkey);
  peer_msg.qpn = ntohl(recv_msg.qpn);
  peer_msg.lid = ntohs(recv_msg.lid);
  memcpy(peer_msg.gid, recv_msg.gid, 16);

  cout << "peer_addr=" << &peer_msg.addr << " peer_rkey=" << peer_msg.rkey
       << " peer_qpn=" << peer_msg.qpn << " peer_lid=" << peer_msg.lid << endl;

  if(argc == 3) {
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    int rc;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)buff;
    sge.length = msg_buff_size;
    sge.lkey = mr->lkey;
    /* prepare the receive work request */
    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    rr.wr_id = 0;
    rr.sg_list = &sge;
    rr.num_sge = 1;
    /* post the Receive Request to the RQ */
    rc = ibv_post_recv(qp, &rr, &bad_wr);
    if (rc)
      cout << "failed to post RR" << endl;
    else
      cout << "Receive Request was posted" << endl;
  }

  { // move to rtr
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_1024;
    attr.dest_qp_num = peer_msg.qpn;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.dlid = peer_msg.lid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.is_global = 1;
    attr.ah_attr.port_num = 1;
    memcpy(&attr.ah_attr.grh.dgid, peer_msg.gid, 16);
    attr.ah_attr.grh.flow_label = 0;
    attr.ah_attr.grh.hop_limit = 1;
    attr.ah_attr.grh.sgid_index = 0;
    attr.ah_attr.grh.traffic_class = 0;
    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                    IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    rc = ibv_modify_qp(qp, &attr, flags);
    if(rc)
      cout << "failed to modify QP state to RTR" << endl;
    else
      cout << "modify QP state to RTR done" << endl;
  }

  { // move qp to rts
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x12;
    attr.retry_cnt = 6;
    attr.rnr_retry = 0;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;
    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                    IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(qp, &attr, flags);
    if(rc)
      cout << "failed to modify QP state to RTS" << endl;
    else
      cout << "modify QP state to RTS done" << endl;
  }

  if(argc != 3) {  // server post send
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc;
    int opcode = IBV_WR_SEND;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)buff;
    sge.length = msg_buff_size;
    sge.lkey = mr->lkey;
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = IBV_WR_SEND;
    sr.send_flags = IBV_SEND_SIGNALED;
    if (opcode != IBV_WR_SEND)
    {
            sr.wr.rdma.remote_addr = peer_msg.addr;
            sr.wr.rdma.rkey = peer_msg.rkey;
    }
    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(qp, &sr, &bad_wr);
    if(rc)
      cout << "failed to post SR" << endl;
    else
      cout << "post SR done" << endl;
  }
  { //polling CQ
    struct ibv_wc wc;
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    int rc = 0;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    do
    {
      poll_result = ibv_poll_cq(cq, 1, &wc);
      gettimeofday(&cur_time, NULL);
      cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
    if (poll_result < 0)
    {
      /* poll CQ failed */
      cout << "poll CQ failed" << endl;
      rc = 1;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
      cout << "completion wasn't found in the CQ after timeout" << endl;
      rc = 1;
    }
    else
    {
      /* CQE found */
      cout << "completion was found in CQ with status 0x" << wc.status << endl;
      /* check the completion status (here we don't care about the completion opcode */
      if (wc.status != IBV_WC_SUCCESS)
      {
         cout <<  "got bad completion with status: , vendor syndrome: " << endl;
         rc = 1;
      }
    }
  }

  if(argc == 3){
    cout << "Message is: " << buff << endl;
  } else {
    strcpy(buff, "I am server");
  }
  if(argc == 3) { //Now the client performs an RDMA read and then write on server.
    { //read
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc;
    int opcode = IBV_WR_RDMA_READ;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)buff;
    sge.length = msg_buff_size;
    sge.lkey = mr->lkey;
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = IBV_WR_RDMA_READ;
    sr.send_flags = IBV_SEND_SIGNALED;
    if (opcode != IBV_WR_SEND)
    {
            sr.wr.rdma.remote_addr = peer_msg.addr;
            sr.wr.rdma.rkey = peer_msg.rkey;
    }
    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(qp, &sr, &bad_wr);
    if(rc)
      cout << "failed to post SR" << endl;
    else
      cout << "post SR done" << endl;

    struct ibv_wc wc;
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    do
    {
      poll_result = ibv_poll_cq(cq, 1, &wc);
      gettimeofday(&cur_time, NULL);
      cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
    if (poll_result < 0)
    {
      /* poll CQ failed */
      cout << "poll CQ failed" << endl;
      rc = 1;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
      cout << "completion wasn't found in the CQ after timeout" << endl;
      rc = 1;
    }
    else
    {
      /* CQE found */
      cout << "completion was found in CQ with status 0x" << wc.status << endl;
      /* check the completion status (here we don't care about the completion opcode */
      if (wc.status != IBV_WC_SUCCESS)
      {
         cout <<  "got bad completion with status: , vendor syndrome: " << endl;
         rc = 1;
      }
    }
    cout << "Contents of server's buffer: " << buff << endl;
    }
    { // write
    strcpy(buff, "I am client");
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc;
    int opcode = IBV_WR_RDMA_WRITE;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)buff;
    sge.length = msg_buff_size;
    sge.lkey = mr->lkey;
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = IBV_WR_RDMA_WRITE;
    sr.send_flags = IBV_SEND_SIGNALED;
    if (opcode != IBV_WR_SEND)
    {
            sr.wr.rdma.remote_addr = peer_msg.addr;
            sr.wr.rdma.rkey = peer_msg.rkey;
    }
    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(qp, &sr, &bad_wr);
    if(rc)
      cout << "failed to post SR" << endl;
    else
      cout << "post SR done" << endl;

    struct ibv_wc wc;
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    do
    {
      poll_result = ibv_poll_cq(cq, 1, &wc);
      gettimeofday(&cur_time, NULL);
      cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
    if (poll_result < 0)
    {
      /* poll CQ failed */
      cout << "poll CQ failed" << endl;
      rc = 1;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
      cout << "completion wasn't found in the CQ after timeout" << endl;
      rc = 1;
    }
    else
    {
      /* CQE found */
      cout << "completion was found in CQ with status 0x" << wc.status << endl;
      /* check the completion status (here we don't care about the completion opcode */
      if (wc.status != IBV_WC_SUCCESS)
      {
         cout <<  "got bad completion with status: , vendor syndrome: " << endl;
         rc = 1;
      }
    }

    }
    
  }
  if(argc != 3) {
    cout << "Contents of server's buffer at momen: " << buff << endl;
  }
  sleep(5);
  if(r == -1) {
    ibv_free_device_list(dev_list);
    ibv_close_device(ctxt);
    ibv_dealloc_pd(pd);
    ibv_destroy_cq(cq); 
    ibv_dereg_mr(mr);
    ibv_destroy_qp(qp);
  }

  free(buff);
  
  ::close(sd);
  ::close(cfd);
  return 0;
}

