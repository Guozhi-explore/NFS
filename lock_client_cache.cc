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

  tprintf("a0 %s\t",id.c_str());
  
  if(this->lock_cache_list.find(lid)==lock_cache_list.end())
  {
    //only permit one thread use rpc to acquire
    add_lockinfo_if_not_exist(lid);
    sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
    this->lock_cache_list.at(lid).waiting_number=this->lock_cache_list.at(lid).waiting_number+1;
    sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  
    // using rpc to acquire 
    tprintf("%s lock size:%u  [client] begin rpc to acquire\n",id.c_str(),lock_cache_list.size()); 
    
    int rpc_response;
    int r;
    tprintf("a1  %s\t",id.c_str());
    this->lock_cache_list.at(lid).status=ACQUIRING;
    rpc_response=this->cl->call(lock_protocol::acquire,lid,id,r);

    //ok: acquire success,retry wait
    if(rpc_response==lock_protocol::OK)
    {
      this->lock_cache_list.at(lid).status=LOCKED;
        return lock_protocol::OK;
    }
    if(rpc_response==lock_protocol::RETRY){
      this->lock_cache_list.at(lid).status=ACQUIRING;
    } 
  }
  else{
    sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
    this->lock_cache_list.at(lid).waiting_number=this->lock_cache_list.at(lid).waiting_number+1;
    sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);  
  }

  //exist cache
  tprintf("a2  %s\t",id.c_str());
  if(this->lock_cache_list.at(lid).status==LOCKED)
  {
    //wait after other thread modified
    tprintf("a3  %s\t",id.c_str());
   //wait for other thread release lock
    tprintf("1\t");
    sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);
    //acquire lock successfully 
  }

  tprintf("a4  %s\t",id.c_str());
  if(this->lock_cache_list.at(lid).status==ACQUIRING)
  {
    //wait for client acquire lock
    tprintf("2\t");
    sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);
    //acquire lock successfully 
  }
  
  // lock is free
  tprintf("a5  %s\t",id.c_str());
  if(this->lock_cache_list.at(lid).status==FREE)
  {
    this->lock_cache_list.at(lid).status=LOCKED;
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
  tprintf("a6  %s\t",id.c_str());
  if(this->lock_cache_list.at(lid).waiting_number!=0){
    sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
    this->lock_cache_list.at(lid).waiting_number=
    this->lock_cache_list.at(lid).waiting_number-1;
    sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  }

  if(this->lock_cache_list.at(lid).needrevoke==true){
    tprintf("a8  %s\t",id.c_str());
    tprintf("[client release%d\n",this->lock_cache_list.at(lid).waiting_number);
    //self own the clock
    if(this->lock_cache_list.at(lid).waiting_number==0&&this->lock_cache_list.at(lid).status==LOCKED)
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
  }
  tprintf("a10  %s\t",id.c_str());
  this->lock_cache_list.at(lid).status=FREE;
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
  tprintf("a11    %s\t",id.c_str());
  sem_wait(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  this->lock_cache_list.at(lid).needrevoke=true;
  sem_post(&this->lock_cache_list.at(lid).mutex_modify_lock_info);
  //release lock immediately as no thread using 
  tprintf("a12  %s\t",id.c_str());
   if(this->lock_cache_list.at(lid).status==FREE&&this
   ->lock_cache_list.at(lid).waiting_number==0)
  {
    int r;  
    tprintf("[revoke erase] %s \n",id.c_str());
    sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);
    this->lock_cache_list.erase(lid);
    tprintf("[revoke erase finish] %s \n",id.c_str());
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
  tprintf("a14  %s\t",id.c_str());
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
    lock_info.status=NONE;
    lock_info.needrevoke=false;
    sem_init(&lock_info.mutex_acquire_lock,0,1);
    sem_init(&lock_info.mutex_modify_lock_info,0,1);
    this->lock_cache_list.insert(std::pair<int,c_lock_info>(lid,lock_info));
    tprintf("3\t");
    sem_wait(&this->lock_cache_list.at(lid).mutex_acquire_lock);
  }
}