// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <map>
#include <semaphore.h>


// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

enum lockstatus {NONE,FREE,LOCKED,ACQUIRING,RELEASING};
struct c_lock_info{
    sem_t mutex_acquire_lock;
    // must acquire modify lock before modify info value
    sem_t mutex_modify_lock_info; 
    sem_t mutex_release_to_server;
    lockstatus status;
    int waiting_number;
    bool needrevoke;
};
class lock_client_cache : public lock_client {
 private:
  
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  std::map<int,c_lock_info> lock_cache_list;
 public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);

  void add_lockinfo_if_not_exist(lock_protocol::lockid_t lid);
};


#endif
