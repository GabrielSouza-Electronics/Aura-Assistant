#include <gui/common/TasksLogic.hpp>
#include <assert.h>
#include <stdio.h>
int main() {
    TasksLogic n; n.enter();
    assert(n.tick(true,1,0,9)==0 && n.page==0); // Entering gesture cannot change page.
    n.tick(false,0,0,9);
    assert(n.tick(true,1,0,9)==1 && n.page==1 && n.arrow==1);
    for(int i=0;i<30;++i) n.tick(true,1,0,9);
    assert(n.page==2); n.tick(true,1,0,9); assert(n.page==2 && n.arrow==0);
    n.tick(false,0,0,9); assert(n.tick(true,-1,0,9)==1 && n.page==1);
    n.tick(false,0,0,9); assert(n.tick(true,0,-1,9)==1 && n.row==1);
    n.tick(false,0,0,9); assert(n.tick(true,0,.6f,9)==1 && n.row==0);
    n.tick(false,0,0,9);
    for(int i=0;i<59;++i) assert(n.tick(true,0,1,9)==0);
    assert(n.tick(true,0,1,9)==0);
    n.enter(); n.tick(false,0,0,0); n.tick(true,1,0,0); assert(n.page==0 && n.arrow==0);
    n.page=7; n.row=3; n.tick(false,0,0,5); assert(n.page==1 && n.row==0);
    puts("Tasks navigation: entry latch, page bounds/repeat/glow, rows, no directional exit and list shrink passed.");
}
