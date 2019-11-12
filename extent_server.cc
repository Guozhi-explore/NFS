// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "handle.h"
#include<tprintf.h>
extent_server::extent_server() 
{
  im = new inode_manager();
}

int extent_server::create(uint32_t type, std::string url,extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  //printf("extent_server: create inode\n");
  id = im->alloc_inode(type);
  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf,std::string url, int &)
{
  id &= 0x7fffffff;
  tprintf("extent server put inode : %u\n",id);
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  //url 为“”表示，这是revoke回来的writeback，并不想cache inode
  /*if(url!="")
  {
    if(this->cache_list.find(id)!=cache_list.end()&&cache_list[id].content_is_cached==true&&cache_list[id].content_cached_url!=url)
    {
      int r;
      //handle(cache_list[id].content_cached_url).safebind()->call(extent_protocol::revoke,id, extent_protocol::DELETE,r);
    }
    cache_list[id].content_cached_url=url;
    cache_list[id].content_is_cached=true;
  }*/
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string url,std::string &buf)
{
  //printf("extent_server: get %lld\n", id);
  tprintf("extent server get inode : %u\n",id);
  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;
  /*if(this->cache_list.find(id)!=cache_list.end()&&cache_list[id].content_is_cached==true&&cache_list[id].content_cached_url!=url)
  {
    int r;
      //handle(cache_list[id].content_cached_url).safebind()->call(extent_protocol::revoke,id,extent_protocol::WRITEBACK,r);
  }*/
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }
  /*this->cache_list[id].content_cached_url=url;
  cache_list[id].content_is_cached=true;*/
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id,std::string url, extent_protocol::attr &a)
{
  //printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));

  /*if(this->cache_list.find(id)!=cache_list.end()&&cache_list[id].attr_is_cached==true&&cache_list[id].attr_cached_url!=url)
  {
      int r;
      //handle(cache_list[id].attr_cached_url).safebind()->call(extent_protocol::revoke,id,extent_protocol::WRITEBACK,r);
      printf("revoke finish\n");
  }*/

  im->getattr(id, attr);
  a = attr;
  /*cache_list[id].attr_cached_url=url;
  cache_list[id].attr_is_cached=true;*/
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  //printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
  /*cacheEntry reset;
  cache_list[id]=reset;*/
  return extent_protocol::OK;
}



