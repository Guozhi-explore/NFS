#include "inode_manager.h"
#include<iostream>
#include<ctime>
using namespace std;

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{

  if(id<0||id>=BLOCK_NUM)
  {
    cout<<"[error message]  "<<"blockid incorrect"<<endl;
    return;
  }
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
    if(id<0||id>=BLOCK_NUM)
    {
      cout<<"[error message]  "<<"blockid incorrect"<<endl;
      return;
    }
    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */

  int start;

  start=IBLOCK(INODE_NUM,BLOCK_SIZE)+1;
  for(;start<BLOCK_NUM;++start)
  {
    if(this->using_blocks[start]==0)
    {
      this->using_blocks[start]=1;
      return start;
    }
  }

  return -1;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  
  this->using_blocks[id]=0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */

  int inum;
  struct inode *inode;

  for(inum=1;inum<INODE_NUM;++inum)
  {
    inode=get_inode(inum);
    if(inode->type==0)
    {
      inode->size=0;
      inode->type=type;
      inode->atime=(unsigned) std::time(0);
      inode->ctime=(unsigned) std::time(0);
      inode->mtime=(unsigned) std::time(0);
      put_inode(inum,inode);
      return inum;
    }
  }
  return -1;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  struct inode *ino;
  
  ino=get_inode(inum);
  if(!ino)
  {
    cout<<"[error message]: inode already freed"<<endl;
    return;
  }
  ino->type=0;
  for(int i=0;i<=NDIRECT;++i)
  {
    this->bm->free_block(ino->blocks[i]);
  }
  
  put_inode(inum,ino);
  free(ino);
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  /*if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }*/

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  struct inode *ino;
  int read_size=0,inum_size,nindirect_index=0;
  char *buf,nindirect_buf[BLOCK_SIZE];
  blockid_t *nindirect_blocks;
  buf=(char *)malloc(BLOCK_SIZE*MAXFILE);
  assert(buf);
  
  ino=get_inode(inum);
  inum_size=ino->size;

  while(read_size<inum_size&&read_size<=(NDIRECT-1)*BLOCK_SIZE)
  {
    this->bm->read_block(ino->blocks[read_size/BLOCK_SIZE],buf+read_size);
    read_size+=BLOCK_SIZE;
  }
  
  /*
  *CASE when file size is too large that need to layer block
  */
  if(inum_size>(NDIRECT*BLOCK_SIZE))
  {
    
    this->bm->read_block(ino->blocks[NDIRECT],nindirect_buf);
    nindirect_blocks=(blockid_t *)nindirect_buf;
    while(read_size<=int((MAXFILE-1)*BLOCK_SIZE))
    {
      this->bm->read_block(nindirect_blocks[nindirect_index],buf+read_size);
      read_size+=BLOCK_SIZE;
      nindirect_index++;
    }
  }

  /*
  *update time
  */
  ino->atime=(unsigned) std::time(0);
  put_inode(inum,ino);

  *buf_out=buf;
  *size=inum_size;
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */

  struct inode *ino;
  int inode_size,write_size=0,nindirect_index=0;
  blockid_t *blocks,nindirect_blocks[NINDIRECT], new_block;

  ino=get_inode(inum);
  inode_size=ino->size;
  blocks=ino->blocks;

  /*
  *case 1:inode_size is larger than size
  */
  while(inode_size>=(size+BLOCK_SIZE))
  {
    this->bm->free_block(blocks[inode_size/BLOCK_SIZE]);
    inode_size-=BLOCK_SIZE;
  }

  while(write_size<inode_size&&write_size<=(NDIRECT-1)*BLOCK_SIZE)
  {
    this->bm->write_block(blocks[write_size/BLOCK_SIZE],buf+write_size);
    write_size+=BLOCK_SIZE;
  }

  /*
  *case 2:inode_size is smaller than size
  */
      /*
      * situation a: one layer block can store the size
      */
  while(write_size<size&&write_size<=(NDIRECT-1)*BLOCK_SIZE)
  {
    new_block=this->bm->alloc_block();
    this->bm->write_block(new_block,buf+write_size);
    ino->blocks[write_size/BLOCK_SIZE]=new_block;
    write_size+=BLOCK_SIZE;
  }

    /*
    *situation b: need two layer block to store the size
    */
  
   if(size>NDIRECT*BLOCK_SIZE)
   {
     while(write_size<=(int)((MAXFILE-1)*BLOCK_SIZE)&&write_size<size)
    {
      new_block=this->bm->alloc_block();
      this->bm->write_block(new_block,buf+write_size);
      nindirect_blocks[nindirect_index]=new_block;
      nindirect_index++;
      write_size+=BLOCK_SIZE;
    }
    // write nindirect array into a block
    new_block=this->bm->alloc_block();
    this->bm->write_block(new_block,(char *)nindirect_blocks);
    ino->blocks[NDIRECT]=new_block;
    
  }
  
  assert(write_size<=int(MAXFILE*BLOCK_SIZE));
  ino->size=size;
  ino->atime=(unsigned) std::time(0);
  ino->ctime=(unsigned) std::time(0);
  ino->mtime=(unsigned) std::time(0);
  put_inode(inum,ino);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode *ino;

  ino=get_inode(inum);
  if(ino->type==0)
  {
    cout<<"[error message]: "<<"inum has not been allocated"<<endl;
  }
  else{
    a.type=ino->type;
    a.size=ino->size;
    a.atime=ino->atime;
    a.ctime=ino->ctime;
    a.mtime=ino->mtime;
  }
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  assert(inum>=0&&inum<=INODE_NUM);
  this->free_inode(inum);
  return;
}
