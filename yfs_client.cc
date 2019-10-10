// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std;

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    string inode_content;

    if(ec->get(ino,inode_content)!=OK)
    {
        printf("get error");
        return IOERR;
    }
    inode_content.resize(size);
    ec->put(ino,inode_content);

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    std::list<dir_content> dir_contents;
    dir_content new_entry;
    string dir_content_buf,new_buf;
    /*
    *check file exist
    */
    bool file_exist=false;
    if(lookup(parent,name,file_exist,ino_out)!=OK)
    {
        cout<<"[yfs test message]: "<<"file exist"<<endl;
        return EXIST;
    }

    /*
    * create file 
    * */
    r=ec->create(extent_protocol::T_FILE,ino_out);

    /*
    *write to parent file
    */
    r=ec->get(parent,dir_content_buf);
    new_entry.inum=ino_out;
    /*
    *get file length
    */
    ostringstream ostr;
    ostr<<name;
    new_entry.filename_size=ostr.str().size();
    memcpy(new_entry.file_name,name,new_entry.filename_size);

    new_buf.assign((char *) (&new_entry), sizeof(struct dir_content));
    dir_content_buf.append(new_buf);
    ec->put(parent,dir_content_buf);
    
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    std::list<dir_content> dir_contents;
    std::list<dirent> dirents;
    dir_content new_entry;
    string dir_content_buf,new_buf;
    /*
    *check file exist
    */
    bool file_exist=false;
    if(lookup(parent,name,file_exist,ino_out)!=OK)
    {
        cout<<"[yfs test message]: "<<"file exist"<<endl;
        return EXIST;
    }
    /*
    * create file or dir
    */
    r=ec->create(extent_protocol::T_DIR,ino_out);
    /*
    *write to parent dir
    */
    r=ec->get(parent,dir_content_buf);
    new_entry.inum=ino_out;
    ostringstream ostr;
    ostr<<name;
    new_entry.filename_size=ostr.str().size();
    memcpy(new_entry.file_name,name,new_entry.filename_size);

    new_buf.assign((char *) (&new_entry), sizeof(struct dir_content));
    dir_content_buf.append(new_buf);
    ec->put(parent,dir_content_buf);
   
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    int dir_exist=0;
    dirinfo dir_info;
    std::list<dirent> entries;

    memset(&dir_info, 0, sizeof(struct dirinfo));

    /*
    *judge dir exist
    */
    if(!isdir(parent))
    {
        found=false;
        return IOERR;
    }
    
    /*
    *get file inum and name in the dir
    */
    if(readdir(parent,entries)==IOERR)
    {
        found=false;
        return IOERR;
    }
    for(std::list<dirent>::iterator it=entries.begin();it!=entries.end();++it)
    {
        if(it->name.compare(name)==0)
        {
            ino_out=it->inum;
            found=true;
            return EXIST;
        }
    }
    
    found=false;
    return OK;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    string buf;
    std::list<dirent> entries;
    std::list<dirent>::iterator dirent_iter=entries.end();
    dir_content parse_entry;
    dirent dir_entry;

    ec->get(dir,buf);
    const char *cbuf=buf.c_str();

    /*
    *parse string to dirent list
    */
    for(int i=0;i<buf.size();i+=sizeof(struct dir_content))
    {
        memcpy(&parse_entry, cbuf + i, sizeof(struct dir_content));
        dir_entry.inum=parse_entry.inum;
        dir_entry.name.assign(parse_entry.file_name,parse_entry.filename_size);
        entries.insert(dirent_iter,dir_entry);
        dirent_iter=entries.end();
    }
    list=entries;
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string inode_content,result_content;
    int file_size;

    ec->get(ino,inode_content);
    file_size=inode_content.size();

    if(off+size>file_size)
    {
        result_content=inode_content.substr(off,size-off);
        data=result_content;
        return r;
    }
    else
    {
        result_content=inode_content.substr(off,size);
        data=result_content;
        return r;
    }
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    string origin_content,add_content;
    int origin_size;

    ec->get(ino, origin_content);
    origin_size=origin_content.size();
    add_content.assign(data, size);
    
    if ((unsigned int) off <= origin_content.size()) {
        origin_content.replace(off, size, add_content);
        bytes_written = size;
    } else {
        origin_content.resize(size + off, '\0');
        origin_content.replace(off, size, add_content);
        bytes_written = size + off - origin_size;
    }
    ec->put(ino,origin_content);
   
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    string buf;
    dir_content parse_entry;
    dirent dir_entry;
    int i=0;

    if(!isdir(parent))
    {
        return IOERR;
    }

    ec->get(parent,buf);
    const char *cbuf=buf.c_str();

    /*
    *parse string to dirent list
    */
    for(i=0;i<buf.size();i+=sizeof(struct dir_content))
    {
        memcpy(&parse_entry, cbuf + i, sizeof(struct dir_content));
        dir_entry.name.assign(parse_entry.file_name,parse_entry.filename_size);
        ostringstream ostr;
        ostr<<name;
        //string.compare() parameter can not be a char*
        if(dir_entry.name.compare(ostr.str())==0)
        {
            break;
        }
    }
        
    if(i<buf.size())
    {
         string::iterator iter=buf.begin();
        buf.erase(iter+i,iter+i+sizeof(dir_content));    
     }
    
    ec->put(parent,buf);
    return r;
}

int yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    int r = OK;
    std::list<dir_content> dir_contents;
    dir_content new_entry;
    string dir_content_buf,new_buf;
    /*
    *check file exist
    */
    bool file_exist=false;
    if(lookup(parent,name,file_exist,ino_out)!=OK)
    {
        cout<<"[yfs test message]: "<<"file exist"<<endl;
        return EXIST;
    }

    /*
    * create file or dir
    * */
    r=ec->create(extent_protocol::T_SYMLINK,ino_out);
    /*
    *write link in inode
    */
    r=ec->put(ino_out,string(link));
    cout<<"[symlink test]:"<<string(link)<<endl;
    /*
    *write to parent dir
    */
    ec->get(parent,dir_content_buf);
    new_entry.inum=ino_out;
    /*
    *get file length
    */
    ostringstream ostr;
    ostr<<name;
    new_entry.filename_size=ostr.str().size();
    memcpy(new_entry.file_name,name,new_entry.filename_size);

    new_buf.assign((char *) (&new_entry), sizeof(struct dir_content));
    dir_content_buf.append(new_buf);
    ec->put(parent,dir_content_buf);

    return r;
}

int yfs_client::readlink(inum ino, std::string &data)
{
    int r = OK;
    string buf;
    r = ec->get(ino, buf);
    cout<<"[symlink test]:"<<buf<<endl;
    data = buf;

    return r;
}

bool yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}