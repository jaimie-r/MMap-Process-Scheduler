#include "ide.h"
#include "ext2.h"
#include "shared.h"
#include "libk.h"
#include "vmm.h"
#include "config.h"
#include "future.h"

/* Called by one CPU */
void kernelMain(void) {

    using namespace VMM;


    // IDE device #1
    auto ide = Shared<Ide>::make(1);
    
    // We expect to find an ext2 file system there
    auto fs = Shared<Ext2>::make(ide);
   
   // get "/"
   auto root = fs->root;

   //  get "/hello"
   auto hello = fs->find(root,"hello");

   auto p = (char*) naive_mmap(hello->size_in_bytes(),hello,0);
   CHECK(p == (char*) 0x80000000);
   Debug::printf("*** 1\n");
   Debug::printf("%s",p);

   

   auto child = future<bool>([] {
      auto p = (uint32_t*) naive_mmap(1,Shared<Node>{},0);
      CHECK(p == (uint32_t*) 0x80000000);
      CHECK(p[0] == 0);
      CHECK(p[1] == 0);
      CHECK(p[2] == 0);
      return true;
   });

   naive_munmap((void*)kConfig.ioAPIC);
   naive_munmap((void*)kConfig.localAPIC);

   auto q = (char*) naive_mmap(hello->size_in_bytes() + 4096,hello,0);
   CHECK(q == (char*) 0x80001000);
   Debug::printf("*** 2\n");
   Debug::printf("%s",q);
   q[0] = '*';
   q[1] = '*';
   q[2] = '*';
   q[4] = 'w';
   q[5] = 'e';
   q[6] = ' ';
   Debug::printf("*** 3\n");
   Debug::printf("%s",q);

   auto fortunes = fs->find(root,"fortunes");
   auto f = (char*) naive_mmap(fortunes->size_in_bytes(),fortunes,0);
   CHECK(f == (char*) 0x80003000);

   auto r = (int*) naive_mmap(1,Shared<Node>{},0);
   CHECK(r == (int*) 0x8000a000);
   Debug::printf("*** %d\n",r[10]);
   r[10] = 42;
   Debug::printf("*** %d\n",r[10]);

   naive_munmap(r);

   auto s = (int*) naive_mmap(1,Shared<Node>{},0);
   CHECK(s == (int*) 0x8000a000);
   Debug::printf("*** %d\n",s[10]);

   child->get();
}

