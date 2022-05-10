#include "Infiniband.h"

Infiniband::~Infiniband() {
  free(buff);
}
int Infiniband::init() {
  port_num = 1;
  cq_depth = 1;
  dev_list = ibv_get_device_list(&num_devices);
  if(dev_list == nullptr || num_devices == 0) {
    cout << "failed to get rdma device list" << endl;
    return -1;
  }
  device = dev_list[0]; //use fisrt device
  ctxt = ibv_open_device(device); // get rdma deives context
  if(ctxt == nullptr) {
    cout << "open rdma device failed" << endl;
    return -1;
  }
  int r = ibv_query_port(ctxt, port_num, &port_attr);
  if(r == -1) {
    cout << "query port failed" << endl;
    return -1;
  }
  r = ibv_query_gid(ctxt, port_num, 0, &gid);
  if(r) {
   cout << "query gid failed" << endl;
   return -1;
  }

  pd = ibv_alloc_pd(ctxt); // alloc protection domain
  if(pd == nullptr) {
    cout << "failed to allocate infiniband protection domain" << endl;
    return -1;
  }
  cq = ibv_create_cq(ctxt, cq_depth, NULL, NULL, 0); // get completion queue
  if(!cq) {
    cout << "failed to create receive completion queue" << endl;
    return -1;
  }

  buffer_size = 1024;
  buff = (char*)malloc(buffer_size);
  memset(buff, 0, buffer_size);
  mr = ibv_reg_mr(pd, buff, buffer_size, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ); // register memory
  if(mr == nullptr) {
    cout << "failed to register memory" << endl;
    return -1;
  }

  is_init = true;
  return 1;
}

int Infiniband::create_qp() {
  int r = 0;
  struct ibv_qp_init_attr qpia;  // ibv_qp_init_attr use to descrip QP attr
  memset(&qpia, 0, sizeof(qpia));
  qpia.qp_type = IBV_QPT_RC; // this is a reliable connection
  qpia.sq_sig_all = 0; // only generate CQEs on requested WQEs
  qpia.send_cq = cq;
  qpia.recv_cq = cq;

  qpia.cap.max_send_wr = 1024; // max outstanding send requests 
  qpia.cap.max_send_sge = 1;   // max send scatter-gather elements

  qpia.cap.max_recv_wr = 4096;
  qpia.cap.max_recv_sge = 1;

  qpia.cap.max_inline_data = 0; // max bytes of immediate data on send queue

  qp = ibv_create_qp(pd, &qpia);
  if(qp == NULL) {
    cout << "failed to create QP" << endl;
    return -1;
  }

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
    return r;
  }
  cout << "successed modify QP state to init" << endl;
  return r;
}

int Infiniband::qp_activate() {
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
  if(rc) {
    cout << "failed to modify QP state to RTR" << endl;
    return rc;
  } else {
    cout << "transition to RTR state successfully." << endl;
  }

  attr.qp_state = IBV_QPS_RTS;
  attr.timeout = 14;
  attr.retry_cnt = 7;
  attr.rnr_retry = 7;
  attr.sq_psn = 0;
  attr.max_rd_atomic = 1;
  flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                  IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
  rc = ibv_modify_qp(qp, &attr, flags);
  if(rc) { 
    cout << "failed to modify QP state to RTS" << endl;
    return rc;
  } else {
    cout << "transition to RTS state successfully." << endl;
  }

}

void Infiniband::display() {
  if(!is_init)
    return;
  cout << "RDMA dev count: " << num_devices << endl;
  cout << "RDMA dev name: " << ibv_get_device_name(device) << endl;
  cout << "ib port num: " << port_num << endl;
  cout << "local id: " << port_attr.lid << endl;
  cout << "memory register info: offset=" << &buff 
       << " length=" << buffer_size << " lkey=" 
       << mr->lkey << " rkey=" << mr->rkey << endl;
}

int Infiniband::post_work_request(int opcode) {
  struct ibv_send_wr sr;
  struct ibv_sge sge;
  struct ibv_send_wr *bad_wr = NULL;
  int rc;
  /* prepare the scatter/gather entry */
  memset(&sge, 0, sizeof(sge));
  sge.addr = (uintptr_t)buff;
  sge.length = buffer_size;
  sge.lkey = mr->lkey;
  /* prepare the send work request */
  memset(&sr, 0, sizeof(sr));
  sr.next = NULL;
  sr.wr_id = 0;
  sr.sg_list = &sge;
  sr.num_sge = 1;
  if(opcode == IBV_WR_SEND) {
    sr.opcode = IBV_WR_SEND;
  } else if(opcode == IBV_WR_RDMA_READ) {
    sr.opcode = IBV_WR_RDMA_READ;
  } else if(opcode == IBV_WR_RDMA_WRITE) {
    sr.opcode = IBV_WR_RDMA_WRITE;
  }
  sr.send_flags = IBV_SEND_SIGNALED;
  if(opcode != IBV_WR_SEND)
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

  return rc;
}

int Infiniband::post_recv() {
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    int rc;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)buff;
    sge.length = buffer_size;
    sge.lkey = mr->lkey;
    /* prepare the receive work request */
    memset(&rr, 0, sizeof(rr));
    rr.next = nullptr;
    rr.wr_id = 0;
    rr.sg_list = &sge;
    rr.num_sge = 1;
    /* post the Receive Request to the RQ */
    rc = ibv_post_recv(qp, &rr, &bad_wr);
    if (rc)
      cout << "failed to post RR" << endl;
    else
      cout << "Receive Request was posted" << endl;
    return rc;
}

int Infiniband::poll_cq() {
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
  if(poll_result < 0) {
    /* poll CQ failed */
    cout << "poll CQ failed" << endl;
    rc = 1;
  } else if(poll_result == 0) { /* the CQ is empty */
    cout << "completion wasn't found in the CQ after timeout" << endl;
    rc = 1;
  } else {
    /* CQE found */
    cout << "completion was found in CQ with status 0x" << wc.status << endl;
    /* check the completion status (here we don't care about the completion opcode */
    if(wc.status != IBV_WC_SUCCESS) {
       cout <<  "got bad completion with status: , vendor syndrome: " << endl;
       rc = 1;
    }
  }
  return rc;
}

int Infiniband::destroy() {
  ibv_destroy_qp(qp);
  ibv_destroy_cq(cq);
  ibv_dereg_mr(mr);
  ibv_dealloc_pd(pd);
  ibv_close_device(ctxt);
}

int Infiniband::send_msg(int sock, int size, char *my_msg, char *peer_msg) {
  int rc;
  int read_bytes = 0;
  int total_read_bytes = 0;
  rc = ::write(sock, my_msg, size);
  if (rc < size)
    cout << "send msg failed" << endl;
  else {
    rc = 0;
    cout << "send SYNMsg done" << endl;
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
    cout << "recv SYNMsg done" << endl;

  return rc;
}

