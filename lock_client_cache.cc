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
        pthread_mutex_init(&ServerLock, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid) {
     /*
    *FREE:CLIENT拿到锁，没有thread拿
    *LOCKED:被一个thread拿走
    ×ACQUIRING:正在拿锁
    * RELEASING:正在归还
    * NONE：不知道锁的情况
    */
    tprintf("acquire by %s\n",id.c_str());
    int r;
    pthread_mutex_lock(&ServerLock);
    if (locks.find(lid) == locks.end()) 
    { 
        locks[lid] = new LockEntry();
    }
    LockEntry *lockEntry = locks[lid];
    sem_wait(&lockEntry->entry_lock);
    ClientThreads *thisThread = new ClientThreads();
    pthread_cond_init(&thisThread->cv,NULL);

    //此时没有线程等待，锁在本地直接拿，不在问server要
    if (lockEntry->threads.empty()) {
        LockState s = lockEntry->state;
        lockEntry->threads.push_back(thisThread);
        switch (s)
        {
        case FREE:
            lockEntry->state = LOCKED;
            sem_post(&lockEntry->entry_lock);
            pthread_mutex_unlock(&ServerLock);
            return lock_protocol::OK;
            break;
        case NONE:
            sem_post(&lockEntry->entry_lock);
            lockEntry->state = ACQUIRING;
            while (lockEntry->state == ACQUIRING) {
                pthread_mutex_unlock(&ServerLock);
                tprintf("send acquire %s",id.c_str());
                int ret = cl->call(lock_protocol::acquire, lid, id, r);
                pthread_mutex_lock(&ServerLock);
                if (ret == lock_protocol::OK) {
                    lockEntry->state = LOCKED;
                    pthread_mutex_unlock(&ServerLock);
                    return lock_protocol::OK;
                } else {
                    if (lockEntry->todo == EMPTY) {
                        pthread_cond_wait(&thisThread->cv, &ServerLock);
                        lockEntry->todo = EMPTY;
                    } else lockEntry->todo = EMPTY;
                }
            }
        default:
            sem_post(&lockEntry->entry_lock);
            pthread_cond_wait(&thisThread->cv, &ServerLock);
            lockEntry->state = ACQUIRING;
            while (lockEntry->state == ACQUIRING) {
                pthread_mutex_unlock(&ServerLock);
                tprintf("send acquire %s",id.c_str());
                int ret = cl->call(lock_protocol::acquire, lid, id, r);
                pthread_mutex_lock(&ServerLock);
                if (ret == lock_protocol::OK) {
                    lockEntry->state = LOCKED;
                    pthread_mutex_unlock(&ServerLock);
                    return lock_protocol::OK;
                } else {
                    if (lockEntry->todo == EMPTY) {
                        pthread_cond_wait(&thisThread->cv, &ServerLock);
                        lockEntry->todo = EMPTY;
                    } else lockEntry->todo = EMPTY;
                }
            }
        }
    }
    else {
        lockEntry->threads.push_back(thisThread);
        pthread_cond_wait(&thisThread->cv, &ServerLock);
        switch (lockEntry->state) {
            case FREE:
		    lockEntry->state = LOCKED;
            case LOCKED:
                sem_post(&lockEntry->entry_lock);
                pthread_mutex_unlock(&ServerLock);
                return lock_protocol::OK;
            case NONE:
                sem_post(&lockEntry->entry_lock);
                lockEntry->state = ACQUIRING;
                while (lockEntry->state == ACQUIRING) {
                    pthread_mutex_unlock(&ServerLock);
                     tprintf("send acquire %s",id.c_str());
                    int ret = cl->call(lock_protocol::acquire, lid, id, r);
                    pthread_mutex_lock(&ServerLock);
                    if (ret == lock_protocol::OK) {
                        lockEntry->state = LOCKED;
                        pthread_mutex_unlock(&ServerLock);
                        return lock_protocol::OK;
                    } else {
                        if (lockEntry->todo == EMPTY) {
                            pthread_cond_wait(&thisThread->cv, &ServerLock);
                            lockEntry->todo = EMPTY;
                        } else lockEntry->todo = EMPTY;
                    }
                }
            default:
                assert(0);
        }
    }
}


lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {

    /*todo为revoke立刻归还，否则将state变为free*/
    //revokek可能在release之前到，也可能在所有release执行完成后到*/
    tprintf("release by %s\n",id.c_str());

    pthread_mutex_lock(&ServerLock);
    LockEntry *lockEntry = locks[lid];
    int r;
    int ret = rlock_protocol::OK;
    bool fromRevoked = false;
    lockEntry->state = FREE;
    //考虑到后面可能没有release的情况，这里直接归还
    if (lockEntry->todo == REVOKE) {
        fromRevoked = true;
        lockEntry->state = RELEASING;
        pthread_mutex_unlock(&ServerLock);
         tprintf("send release %s",id.c_str());
        ret = cl->call(lock_protocol::release, lid, id, r);
        pthread_mutex_lock(&ServerLock);
        lockEntry->todo = EMPTY;
        lockEntry->state = NONE;
    }

    tprintf("erase %s",id.c_str());
    delete lockEntry->threads.front();
    lockEntry->threads.pop_front();
    //还有线程在等待，保持locked
    if (lockEntry->threads.size() >= 1) {
        if (!fromRevoked) lockEntry->state = LOCKED;
        pthread_cond_signal(&lockEntry->threads.front()->cv);
    }
    sem_post(&lockEntry->entry_lock);
    pthread_mutex_unlock(&ServerLock);
    return ret;

}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                  int &) {  
    tprintf("receive revoke from %s",id.c_str());
    int r;
    int ret = rlock_protocol::OK;
    pthread_mutex_lock(&ServerLock);
    LockEntry *lockEntry = locks[lid];
    //如果现在已经是free，释放掉锁
    if (lockEntry->state == FREE) {
        lockEntry->state = RELEASING;
        pthread_mutex_unlock(&ServerLock);
        tprintf("send release %s",id.c_str());
        ret = cl->call(lock_protocol::release, lid, id, r);
        pthread_mutex_lock(&ServerLock);
        lockEntry->state = NONE;
        if (lockEntry->threads.size() >= 1) {
            pthread_cond_signal(&lockEntry->threads.front()->cv);
        }
    } else { lockEntry->todo = REVOKE; }
    pthread_mutex_unlock(&ServerLock);
    return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                 int &) {
    //打开一个在等待的线程的线程锁      
    tprintf("receive retry from %s",id.c_str());                       
    int ret = rlock_protocol::OK;
    pthread_mutex_lock(&ServerLock);
    LockEntry *lockEntry = locks[lid];
    lockEntry->todo = RETRY;
    pthread_cond_signal(&lockEntry->threads.front()->cv);
    pthread_mutex_unlock(&ServerLock);
    return ret;
}


lock_client_cache::~lock_client_cache() {
}




