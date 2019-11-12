// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "inode_manager.h"
using namespace std;
class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;

private:
 /* struct cacheEntry{
    bool content_is_cached;
    bool attr_is_cached;
    string content_cached_url;
    string attr_cached_url;
    cacheEntry(){
      content_is_cached=false;
      attr_is_cached=false;
      content_cached_url="";
      attr_cached_url="";
    }
  };*/
 public:
  extent_server();

  int create(uint32_t type, std::string url,extent_protocol::extentid_t &id);
  int put(extent_protocol::extentid_t id, std::string, std::string url,int &);
  int get(extent_protocol::extentid_t id, std::string url,std::string &);
  int getattr(extent_protocol::extentid_t id, std::string url,extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
private:
 // map<extent_protocol::extentid_t ,cacheEntry > cache_list; 
};

#endif 







