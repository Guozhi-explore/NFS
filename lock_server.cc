// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t 
  lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t 
lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  
  // the lock has not been appeared before
  
  if(this->locks.find(lid)==this->locks.end())
  {
    printf("[lock server]: new lock%d\n",lid);
    sem_t mutex;
    this->locks.insert(std::pair<int,sem_t>(lid,mutex));
    sem_init(&locks.at(lid),0,1);
  }

  //std::cout<<locks.find(lid)->second<<'\t'<<LOCKED<<'\n';
  //use semaphore to mutual exclusion
  /*while(this->locks.find(lid)->second==LOCKED)
  {
    // printf("wait");
    //spin
  }
  //the lock exist and is free
  if(this->locks.find(lid)->second==FREE)
  {
    this->locks.find(lid)->second=LOCKED;
  }
  sem_post(&mutex);
  */
  sem_wait(&this->locks.at(lid));
  //printf("[lock_sever]:%d\t",mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t
 lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  /*if(this->locks.find(lid)!=locks.end())
  {
    this->locks.find(lid)->second=FREE;
  }
  printf("[lock_server]: realease\n");*/
  if(this->locks.find(lid)!=locks.end())
  sem_post(&this->locks.at(lid));
  return ret;
}
