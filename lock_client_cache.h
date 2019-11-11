// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <pthread.h>
#include <map>
#include <list>
#include<semaphore.h>

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
public:
    virtual void dorelease(lock_protocol::lockid_t) = 0;

    virtual ~lock_release_user() {};
};


//

class lock_client_cache : public lock_client {
    /*
    none: client knows nothing about this lock
    free: client owns the lock and no thread has it
    locked: client owns the lock and a thread has it
    acquiring: the client is acquiring ownership
    releasing: the client is releasing ownership
    */
    enum LockState {NONE,ACQUIRING,FREE,RELEASING,LOCKED,};
    enum TODO {EMPTY,RETRY,REVOKE,KEEP};

    struct ClientThreads { pthread_cond_t cv;};

    struct LockEntry {
        LockState state;    //锁现在的状态
        TODO todo;    //锁下一步的动作

        std::list<ClientThreads *> threads;   //线程锁
        sem_t entry_lock;  //修改entry时用锁保护
        LockEntry() {
            state = NONE;todo = EMPTY;
            sem_init(&entry_lock,0,1);}
    };
    std::map<lock_protocol::lockid_t, LockEntry *> locks;

private:
    class lock_release_user *lu;
    int rlock_port;
    std::string hostname;
    std::string id;
    pthread_mutex_t ServerLock;
public:
    static int last_port;

    lock_client_cache(std::string xdst, class lock_release_user *l = 0);

    virtual ~lock_client_cache();

    lock_protocol::status acquire(lock_protocol::lockid_t);

    lock_protocol::status release(lock_protocol::lockid_t);

    rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
                                          int &);

    rlock_protocol::status retry_handler(lock_protocol::lockid_t,
                                         int &);
};


#endif