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
    sem_init(&lock_manager,0,1);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, 
              std::string id, 
                               int &)
{
    sem_wait(&this->lock_manager);
  lock_protocol::status ret = lock_protocol::OK;
  if(this->locks.find(lid)==this->locks.end())
  {
    server_lock_info linfo;
    //lockinfo init
    linfo.clientiswaiting=false;
    linfo.clockisusing=false;
    linfo.lock_state=NONE;
    this->locks.insert(std::pair<int,server_lock_info>(lid,linfo));
    sem_init(&this->locks.at(lid).acquire_lock_mutex,0,1);
    sem_init(&this->locks.at(lid).modify_lock_info_mutex,0,1);
  }
  switch (this->locks.at(lid).lock_state)
  {
    case NONE:
        this->locks.at(lid).lock_state=LOCKED;
        this->locks.at(lid).lock_owner=id;
        sem_post(&this->lock_manager);
      break;
    case LOCKED:
        this->locks.at(lid).lockwaiters.push_back(id);
        this->locks.at(lid).lock_state=REVOKING;
        sem_post(&this->lock_manager);
        rpc_call(this->locks.at(lid).lock_owner,lid,rlock_protocol::revoke);
        break;
    case REVOKING:
        this->locks.at(lid).lockwaiters.push_back(id);
        sem_post(&lock_manager);
    case RETRYING:
        //the first waiting client
        if(id.compare(this->locks.at(lid).lockwaiters[0])==0)
        {
            this->locks.at(lid).retry_client.clear();
            this->locks.at(lid).lock_state=LOCKED;
            this->locks.at(lid).lock_owner=id;
            if(this->locks.at(lid).lockwaiters.size()>0)
            {
                this->locks.at(lid).lock_state=REVOKING;
                sem_post(&lock_manager);
                this->rpc_call(id,lid,rlock_protocol::revoke);
            }
            else{
                sem_post(&lock_manager);
            }
        }
        else{
            this->locks.at(lid).lockwaiters.push_back(id);
            sem_post(&lock_manager);
        }
  default: 
      break;
  }
  return lock_protocol::OK;
}
  

int 
lock_server_cache::release(lock_protocol::lockid_t lid, 
std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  if(id.compare(this->locks.at(lid).lock_owner)!=0)
  {
    //release a lock that not occupied by him, error
    return lock_protocol::IOERR;
  }
  else{
    //send retry to next wait client
    if(this->locks.at(lid).lockwaiters.size()==0)
    {
        return lock_protocol::OK;
    }
    else{
        sem_wait(&lock_manager);
        string retry_client=this->locks.at(lid).lockwaiters[0];
        this->locks.at(lid).lockwaiters.erase(this->locks.at(lid).lockwaiters.begin());
        this->locks.at(lid).retry_client=retry_client;
        this->locks.at(lid).lock_owner.clear();
        this->locks.at(lid).lock_state=RETRYING;
        sem_post(&this->lock_manager);
        this->rpc_call(retry_client,lid,rlock_protocol::retry);
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

void lock_server_cache::rpc_call(string client_id,lock_protocol::lockid_t lid,rlock_protocol::rpc_numbers func)
{
    sockaddr_in dstsock;
    make_sockaddr(client_id.c_str(), &dstsock);
    server = new rpcc(dstsock);
    int r;
    this->server->call(func,lid,r);
}