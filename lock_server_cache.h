#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <semaphore.h>
#include <vector>
using namespace std;

struct server_lock_info{
  std::string lock_owner;
  vector<string> lockwaiters;
  bool clientiswaiting;
  bool clockisusing;
  sem_t modify_lock_info_mutex;
  sem_t acquire_lock_mutex;
};

class lock_server_cache {
 private:
  int nacquire;
  rpcc *server;
  std::map<int,server_lock_info> locks;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
