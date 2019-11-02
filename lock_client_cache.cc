// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;

  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, 
    &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, 
    &lock_client_cache::retry_handler);
}


lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{

  /*
  * 根据lid的锁是不是在本地来决定是否发起rpc
  * 1.如果在其他进程里，等待
  * 2.如果在本地且没有被使用，占用
  * 3.如果本地没有这个锁，发起rpc请求
  * 
  * 根据rpc结果不同处理
  * 1.OK：成功
  * 2.RETRY：锁被其他进程用了
  */
  //本地no cache

  if(this->lock_cache_list.find(lid)==lock_cache_list.end())
  {
    add_lockinfo_if_not_exist(lid);
    //only permit one thread use rpc to acquire
    sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);

    int rpc_response;
    // using rpc to acquire 
    tprintf("%s lock size:%u  [client] begin rpc to acquire\n",id.c_str(),
    lock_cache_list.size()); 

    int r;
    rpc_response=this->cl->call(lock_protocol::acquire,lid,id,r);

    if(rpc_response
      ==lock_protocol::OK)
    {
        return lock_protocol::OK;
    }
    if(rpc_response==lock_protocol::RETRY)
    {
      tprintf("[client receive RETRY %s\n",id.c_str());
      sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      this->lock_cache_list.at(lid).status=ACQUIRING;
      sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);
    }
    else return lock_protocol::IOERR;
  }

  //exist cache
    c_lock_info lock_info;
    lock_info=this->lock_cache_list.at(lid);
    
    if(lock_info.status==LOCKED)
    {
      //wait after other thread modified
      sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      this->lock_cache_list.at(lid).waiting_number=this->lock_cache_list
      .at(lid).waiting_number+1;
      sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      //wait for other thread release lock
      sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);
      //acquire lock successfully 
    }

    if(lock_info.status==ACQUIRING)
    {
      //wait for client acquire lock
      sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);
      //acquire lock successfully 
      tprintf("ACQUIRING SUCCESS%s",this->id.c_str());
    }
    
    // lock is free
    if(lock_info.status==FREE)
    {
      sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      this->lock_cache_list.at(lid).status=LOCKED;
      sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      return lock_protocol::OK;
    }

  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  /*
  * 1.如果本地其他进程还在等待这把锁，唤醒该进程
  * 2.如果本地没有进程需要，用rpc归还
  * */
  if(this->lock_cache_list.at(lid).needrevoke==false)
  {
    if(this->lock_cache_list.at(lid).waiting_number!=0){
      sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      this->lock_cache_list.at(lid).waiting_number=
      this->lock_cache_list.at(lid).waiting_number-1;
      sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
    }
  }
  else{
    tprintf("[client release%d\n",this->lock_cache_list.at(lid).waiting_number);
    if(this->lock_cache_list.at(lid).waiting_number==0)
    {
      //本地无线程在等待，归还给server
      int r;
      
      this->lock_cache_list.erase(lid);
      if(this->cl->call(lock_protocol::release,lid,id,r)
        !=lock_protocol::OK)
      {
        return lock_protocol::IOERR;
      }
      else {
        tprintf("%s release finish\n",id.c_str());
        return lock_protocol::OK;
      }
    }
    else{
      if(this->lock_cache_list.at(lid).waiting_number!=0){
      sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
      this->lock_cache_list.at(lid).waiting_number=
      this->lock_cache_list.at(lid).waiting_number-1;
      sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
     }
    }
  }
  sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  this->lock_cache_list.at(lid).status=FREE;
  sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  sem_post(&this->lock_cache_list.at(lid).mutex_acquire_lock);

  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{

  /*
  * 1.如果没有进程在使用，立刻归还
  * 2.如果有进程使用，用完就还
  * 3.lid is not exist in cache:means revoke if before retry.
  *   add a record of lid
  * */
  int ret = rlock_protocol::OK;
  tprintf("[client:  %s   receive revoke\n",id.c_str());
  sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  this->lock_cache_list.at(lid).needrevoke=true;
  sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  //release lock immediately as no thread using 
   if(this->lock_cache_list.at(lid).status==FREE&&this
   ->lock_cache_list.at(lid).waiting_number==0)
  {
    int r;  
    this->lock_cache_list.erase(lid);
    if(this->cl->call(lock_protocol::release,lid,id,r)
      !=lock_protocol::OK)
    {
      return lock_protocol::IOERR;
    }
    else {
      tprintf("%s release finish\n",id.c_str());
    }
  }
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  /*
  * acquire
  * */
  int ret = rlock_protocol::OK;
  tprintf("[client  %s   receive retry\n",id.c_str());
  //retry may show up before response of acquire
  add_lockinfo_if_not_exist(lid);
  sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  this->lock_cache_list.at(lid).status=FREE;
  sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  sem_post(&this->lock_cache_list.at(lid).mutex_acquire_lock);
  return ret;
}

void lock_client_cache::add_lockinfo_if_not_exist(lock_protocol::lockid_t lid)
{
  if(lock_cache_list.find(lid)==lock_cache_list.end())
  {
    c_lock_info lock_info;
    //init lock_info
    lock_info.waiting_number=0;
    lock_info.status=LOCKED;
    lock_info.needrevoke=false;
    sem_init(&lock_info.mutex_acquire_lock,0,1);
    sem_init(&lock_info.mutex_modify_lock_info,0,1);
    this->lock_cache_list.insert(std::pair<int,c_lock_info>(lid,lock_info));
  }
}