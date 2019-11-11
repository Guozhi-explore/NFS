// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include<tprintf.h>

int extent_client::last_port=1;


extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

  srand(time(NULL)^last_port);
  client_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << client_port;
  url = host.str();
  last_port = client_port;
  rpcs *ecrpc=new rpcs(client_port);
  ecrpc->reg(extent_protocol::revoke,this,&extent_client::revoke_handler);
  tprintf("%s\n",url.c_str());
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  /*extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  ret=cl->call(extent_protocol::create,type,id);
  return ret;*/
  printf("[ec create]\n");
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  int r;
  ret=cl->call(extent_protocol::create,type,url,id);
  cache_list[id].eid_status=FREE;
  return ret; 

}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  //origin version
  /*printf("[ec get]\n");
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
   ret=cl->call(extent_protocol::get,eid,buf);
   return ret;*/

   printf("[ec get ]of %d\n",eid);
   extent_protocol::status ret = extent_protocol::OK;
  if(this->cache_list.find(eid)!=cache_list.end()&&
  cache_list[eid].contain_content)
  {
    printf("ex get use cache\n");
    buf=this->cache_list[eid].content;
    return ret;
  }
  printf("ex get rpc\n");
  int r;
  ret=cl->call(extent_protocol::get,eid,url,buf);
  this->cache_list[eid].eid_status=FREE;
  this->cache_list[eid].contain_content=true;
  this->cache_list[eid].content=buf;
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  /*printf("[ec getattr]\n");
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret*/
  printf("[ec getattr] of %d\n",eid);
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  if(this->cache_list.find(eid)!=cache_list.end()&&cache_list[eid].contain_attr&&
  !cache_list[eid].modify_content)
  {
    attr=this->cache_list[eid].eid_attr;
    return ret;
  }
  if(this->cache_list.find(eid)!=cache_list.end()&&cache_list[eid].modify_content)
  {
    cl->call(extent_protocol::put,eid,this->cache_list[eid].content,url,r);
    cache_list[eid].modify_content=false;
  }
  printf("ex getattr send rpc\n");
  ret = cl->call(extent_protocol::getattr, eid,url, attr);
  this->cache_list[eid].eid_status=FREE;
  this->cache_list[eid].contain_attr=true;
  this->cache_list[eid].eid_attr=attr;
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  printf("[ec put ] towards %d\n content %s\n",eid,buf.c_str());
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  int r;
  if(this->cache_list.find(eid)!=cache_list.end())
  {
    this->cache_list[eid].content=buf;
    this->cache_list[eid].modify_content=true;
    this->cache_list[eid].contain_content=true;
    return ret;
  }                                                                                                                                           
  ret=cl->call(extent_protocol::put,eid,buf,url,r);
  this->cache_list[eid].eid_status=FREE;
  this->cache_list[eid].contain_content=true;
  this->cache_list[eid].content=buf;
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  int r;
  ret=cl->call(extent_protocol::remove,eid,r);
  return ret;
}

extent_protocol::status
extent_client::revoke_handler(extent_protocol::extentid_t eid,
uint32_t type, int &){
  tprintf("%s receive revoke type :%d\n",url.c_str(),type);
  int r;
  int ret=extent_protocol::OK;

  if(this->cache_list.find(eid)==cache_list.end())
  {
    tprintf("%s receive revoke but haven't cached it\n",url.c_str());
    return ret;
  }

  //wait for current using function finish
  if(this->cache_list[eid].contain_content&&
  this->cache_list[eid].modify_content&&
  type==extent_protocol::WRITEBACK)
  {
    string empty="";
    cl->call(extent_protocol::put,eid,cache_list[eid].content,empty,r);
  }
  //reset cache entry
  {
    cache_list[eid].eid_status=NONE;
    cache_list[eid].contain_attr=false;
    cache_list[eid].contain_content=false;
    cache_list[eid].modify_content=false;
  }
  printf("%s loss cache of eid:%d",url.c_str(),eid);
  return ret;
}
