#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "operations.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"cancel ordering failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static InferenceOperation *worker;
static int observed;
/* Only operations.c is compiled with its fsync calls redirected here. No test
 * hook enters the production API. At the marker boundary, act as a worker that
 * observes cancellation and commits its terminal result before the caller's
 * cancellation response is read. The database acknowledgement must already be
 * committed and visible through this independent worker connection. */
int operations_cancel_test_fsync(int fd){
    int result=fsync(fd);if(result||!worker)return result;struct stat a,b;
    CHECK(!fstat(fd,&a));CHECK(!fstat(worker->cancel,&b));
    if(a.st_dev!=b.st_dev||a.st_ino!=b.st_ino)return result;
    char marker=0;CHECK(pread(worker->cancel,&marker,1,0)==1&&marker=='1');
    sqlite3_stmt *stmt=NULL;CHECK(sqlite3_prepare_v2(worker->db,"SELECT cancel_requested FROM operations WHERE owner=? AND id=?",-1,&stmt,NULL)==SQLITE_OK);
    CHECK(sqlite3_bind_text(stmt,1,worker->owner,-1,SQLITE_TRANSIENT)==SQLITE_OK);
    CHECK(sqlite3_bind_text(stmt,2,worker->id,-1,SQLITE_TRANSIENT)==SQLITE_OK);
    CHECK(sqlite3_step(stmt)==SQLITE_ROW);CHECK(sqlite3_column_int(stmt,0)==1);CHECK(sqlite3_finalize(stmt)==SQLITE_OK);
    CHECK(!operation_finish(worker,"{\"contract_version\":1,\"error\":{\"code\":\"cancelled\"}}",1));observed++;return result;
}
int main(void){
    char path[]="/tmp/samosa-cancel-order-XXXXXX";CHECK(mkdtemp(path));InferenceConfig cfg={.owner="alice",.admission_state=path};
    InferenceOperation operation;char *saved=NULL;CHECK(!operation_begin(&cfg,"race","bound-request",&operation,&saved));CHECK(!saved);
    char *body=NULL;size_t size=0;FILE *out=open_memstream(&body,&size);CHECK(out);worker=&operation;
    CHECK(!operation_public(&cfg,"operation_cancel","race",out));worker=NULL;CHECK(!fclose(out));CHECK(observed==1);
    CHECK(strstr(body,"\"status\":\"cancelled\"")&&strstr(body,"\"cancel_requested\":true"));CHECK(operation_cancelled(&operation));free(body);
    /* A repeated request preserves the terminal acknowledgement and never
     * sends another marker to an already-finished operation. */
    body=NULL;out=open_memstream(&body,&size);CHECK(out);worker=&operation;CHECK(!operation_public(&cfg,"operation_cancel","race",out));worker=NULL;CHECK(!fclose(out));CHECK(observed==1);CHECK(strstr(body,"\"cancel_requested\":true"));free(body);
    operation_close(&operation);int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    const char *names[]={"operations.sqlite3","operations.lock","operation-0.lock","operation-0.cancel"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);i++)CHECK(!unlinkat(root,names[i],0));CHECK(!close(root));CHECK(!rmdir(path));
    printf("PASS cancellation ordering: %u checks; durable request precedes marker visibility and immediate worker completion\n",checks);return 0;
}
