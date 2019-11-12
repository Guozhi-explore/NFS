// extent client interface.

#ifndef extent_client_h
#define extent_client_h
#include "lock_client_cache.h"
#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
#include<string>
#include<map>



class extent_client {
 private:
  rpcc *cl;
  int client_port;
  std::string hostname;
  std::string url;


 enum status{
   USING,
   FREE,
   NONE
 };
 enum type{
   DIR,
   NORMAL
 };

  struct cacheEntry{
    status eid_status;
    bool contain_content;
    bool contain_attr;
    std::string content;
    extent_protocol::attr eid_attr;
    bool modify_content;
    type eid_type;
    extent_protocol::extentid_t parent;
    cacheEntry(){
      contain_content=false;
      contain_attr=false;
      modify_content=false;
      eid_status=NONE;
      eid_attr.type=extent_protocol::T_DIR;
    }
  };
 public:
  static int last_port;

  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  
  extent_protocol::status revoke_handler(extent_protocol::extentid_t eid,uint32_t type, int &);
  int disable_cache(extent_protocol::extentid_t eid);

private:
  map<extent_protocol::extentid_t,cacheEntry> cache_list;
};


class extent_release:public lock_release_user{
    public: 
    extent_release(extent_client *ec){
        this->ec_release=ec;
    }
    void dorelease(lock_protocol::lockid_t);
    private:
    extent_client *ec_release;
};

#endif 

