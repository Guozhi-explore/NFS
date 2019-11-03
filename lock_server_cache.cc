// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, 
              std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  tprintf("client %s want to acquire lock\n",id.c_str());
  if(this->locks.find(lid)==this->locks.end())
  {
    server_lock_info linfo;
    //lockinfo init
    linfo.clientiswaiting=false;
    linfo.clockisusing=false;
    this->locks.insert(std::pair<int,server_lock_info>(lid,linfo));
    sem_init(&this->locks.at(lid).acquire_lock_mutex,0,1);
    sem_init(&this->locks.at(lid).modify_lock_info_mutex,0,1);
  }
  if(this->locks.at(lid).clockisusing==true&&this->locks.at(lid).clientiswaiting==true)
  {
    //keep id in the queue in preparing to retry
    sem_wait(&this->locks.at(lid).modify_lock_info_mutex);
    this->locks.at(lid).lockwaiters.push_back(id);
    sem_post(&this->locks.at(lid).modify_lock_info_mutex);
    return lock_protocol::RETRY;
  }
  if(this->locks.at(lid).clockisusing==true&&this->locks.at(lid).clientiswaiting==false)
  {
    sem_wait(&this->locks.at(lid).modify_lock_info_mutex);
    this->locks.at(lid).clientiswaiting=true;
    this->locks.at(lid).lockwaiters.push_back(id);
    sem_post(&this->locks.at(lid).modify_lock_info_mutex);
    sockaddr_in dstsock;
    make_sockaddr(this->locks.at(lid).lock_owner.c_str(), &dstsock);
    server = new rpcc(dstsock);
    if (server->bind() < 0) {
      printf("lock_server_cache: call bind error\n");
      return lock_protocol::IOERR;
    }
    int r;
    tprintf("send revoke to %s\n",this->locks.at(lid).lock_owner.c_str());
    //send revoke to lock owner
    if(this->server->call(rlock_protocol::revoke,lid,r)
      ==lock_protocol::OK)
      {
        return lock_protocol::RETRY;
      }
    else{
      return lock_protocol::IOERR;
    }
  }
  //acquire lock successfully
  sem_wait(&this->locks.at(lid).modify_lock_info_mutex);
  locks.at(lid).lock_owner=id;
  locks.at(lid).clockisusing=true;
  sem_post(&this->locks.at(lid).modify_lock_info_mutex);
  tprintf("acquire lock success   %s\n",id.c_str());
  
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, 
std::string id, 
         int &r)
{
  tprintf("current lock owner %s\n",this->locks.at(lid)
    .lock_owner.c_str());
  lock_protocol::status ret = lock_protocol::OK;
  if(id.compare(this->locks.at(lid).lock_owner)!=0)
  {
    //release a lock that not occupied by him, error
    return lock_protocol::IOERR;
  }
  else{
    //send retry to next wait client
    //once clien receive a retry from server,he acquire the lock
    //simutaniously. no rpc is needed agian
     if(this->locks.at(lid).clientiswaiting==true){
      sockaddr_in dstsock;
      string next_client_id=this->locks.at(lid).lockwaiters[0];
      make_sockaddr(next_client_id.c_str(), &dstsock);
      server = new rpcc(dstsock);
      if (server->bind() < 0) {
        printf("lock_server_cache: call bind error\n");
        return lock_protocol::IOERR;
      }

      sem_wait(&this->locks.at(lid).modify_lock_info_mutex);
      this->locks.at(lid).lockwaiters.erase(
        this->locks.at(lid).lockwaiters.begin());
      this->locks.at(lid).clientiswaiting=(
        (this->locks.at(lid).lockwaiters.size()==0)?false:true
      );
      this->locks.at(lid).lock_owner=next_client_id;
       sem_post(&this->locks.at(lid).modify_lock_info_mutex);
      int r;
      tprintf("[server] send retry to %s\n",
        next_client_id.c_str());
      if(server->call(rlock_protocol::retry
      ,lid,r)==lock_protocol::IOERR)
      {
        return lock_protocol::IOERR;
      }
      //if still exists other client in the waiting queue, send revoke
      if(this->locks.at(lid).clientiswaiting)
      {   
        tprintf("[server] send revoke to %s\n",
          next_client_id.c_str());
        if(server->call(rlock_protocol::revoke,lid,r)==
            lock_protocol::IOERR)
            {
              return lock_protocol::IOERR;
            }
      }
    }
    else{
      sem_wait(&this->locks.at(lid).modify_lock_info_mutex);
      this->locks.at(lid).clockisusing=false;
      this->locks.at(lid).lock_owner="";
      sem_post(&this->locks.at(lid).modify_lock_info_mutex);
    }
  }
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

