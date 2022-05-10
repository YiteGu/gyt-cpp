#include "Infiniband.h"

using namespace std;

int main(int argc, char *argv[])
{
  bool is_server = false;
  int listen_fd;
  int cfd;
  int sd;
  if(argc == 3) {
    cout << "--------------------------" << endl;
    cout << "Client Endpoint" << endl;
    cout << "--------------------------" << endl;
  } else if(argc == 2) {
    cout << "--------------------------" << endl;
    cout << "Server Endpoint" << endl;
    cout << "--------------------------" << endl;
    is_server = true;
  } else {
    cout << "Parameter error" << endl;
    return -1;
  }
  if(is_server) {
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(6800);
    
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    ::bind(listen_fd, (struct sockaddr*)&sin, sizeof(sin));
    ::listen(listen_fd, 512);
    cout << "waiting for client connection..." << endl;
    sd = ::accept(listen_fd, NULL, 0);
  } else {
    struct sockaddr_in cin;
    struct hostent *h;
    cfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&cin, 0, sizeof(cin));
    cin.sin_family = AF_INET;
    cin.sin_port = htons(6800);
    h = gethostbyname(argv[2]);
    memcpy(&cin.sin_addr, h->h_addr, h->h_length);
    ::connect(cfd, (struct sockaddr*)&cin, sizeof(cin));
  }
  cout << "TCP established" << endl;

  Infiniband ib;
  struct IBSYNMsg recv_msg;
  int r;
  r = ib.init();
  if(r < 0) {
    cout << "ib init failed" << endl;
    goto fail;
  }
  ib.display();
  ib.create_qp();

  ib.my_msg.addr = htonll((uintptr_t)ib.buff);
  ib.my_msg.rkey = htonl(ib.mr->rkey);
  ib.my_msg.qpn = htonl(ib.qp->qp_num);
  ib.my_msg.lid = htons(ib.port_attr.lid);  
  memcpy(ib.my_msg.gid, &ib.gid, 16);

  if(!is_server)   
    ib.send_msg(cfd, sizeof(struct IBSYNMsg), (char*)&ib.my_msg, (char*)&recv_msg);
  else 
    ib.send_msg(sd, sizeof(struct IBSYNMsg), (char*)&ib.my_msg, (char*)&recv_msg);

  ib.peer_msg.addr = ntohll(recv_msg.addr);
  ib.peer_msg.rkey = ntohl(recv_msg.rkey);
  ib.peer_msg.qpn = ntohl(recv_msg.qpn);
  ib.peer_msg.lid = ntohs(recv_msg.lid);
  memcpy(ib.peer_msg.gid, recv_msg.gid, 16);

  if(!is_server) {
    ib.post_recv();
  }
  ib.qp_activate();
  if(is_server) {  // server post send
    ib.post_work_request(IBV_WR_SEND);
  }
  //polling CQ
  ib.poll_cq();

  if(is_server){
    strcpy(ib.buff, "I am server"); // modify buffer context of server
  }

  if(!is_server) { //client run RDMA read and write.
    //read
    ib.post_work_request(IBV_WR_RDMA_READ);
    ib.poll_cq(); // completed read request when poll end
    cout << "get server's buffer context by client: " << ib.buff << endl;
    
    // write
    strcpy(ib.buff, "I am client"); //modify client buffer context, and write to remote memory space
    ib.post_work_request(IBV_WR_RDMA_WRITE);
    ib.poll_cq(); // completed write request when poll end
  }
  if(is_server) {
    sleep(5);
    cout << "Contents of server's buffer at momen: " << ib.buff << endl;
  }
  sleep(5);

fail:
  ib.destroy();
  ::close(sd);
  ::close(cfd);
  return 0;
}

