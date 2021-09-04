/*
Name - Ayush Tiwari
Roll No - 17CS10056

Name - Ritikesh Gupta
Roll No - 17CS30027

Kernel Version - 5.2.6

For running testcase filename = partb_1_17CS10056_17CS30027

*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define NONE 0x00
#define MIN_HEAP 0xFF
#define MAX_HEAP 0xF0

#define MAX 101
#define INF 100000


typedef struct pcb_ {
    pid_t proc_pid;     // Stores the processID of the owner process
    int32_t heapType;   // Stores OxFF(MIN_HEAP) or 0xF0(MAX_HEAP)
    int32_t curSize;    // Stores the dynamic size of the heap
    int32_t heapSize;   // Stores the max size of the heap
    int32_t heap[101];  // Stores the elements of heap
    struct pcb_* next;  // Stores link to next Block in the  list
}pcb;

static struct file_operations file_ops;
static DEFINE_MUTEX(pcb_mutex);
static pcb* pcb_Head = NULL;


MODULE_LICENSE("GPL");

static pcb* 
pcb_node_Create(pid_t pid) 
{
    pcb* proc = (pcb*)kmalloc(sizeof(pcb), GFP_KERNEL);

    proc->proc_pid = pid;
    proc->heapType = NONE;
    proc->curSize = 0;
    proc->heapSize = 0;
    int i;
    for(i = 0 ; i <= 100 ; i++) {
        proc->heap[i] = 0;
    }
    proc->next = NULL;

    return proc;
}

static void
pcb_node_Reset(pcb* proc)
{
    proc->heapType = NONE;
    proc->curSize = 0;
    proc->heapSize = 0;
    int i;
    for(i = 0 ; i <= 100 ; i++) {
        proc->heap[i] = 0;
    }

    return;
}

static pcb* 
pcb_list_Insert(pid_t pid) 
{
    pcb* node = pcb_node_Create(pid);

    node->next = pcb_Head;
    pcb_Head = node;
 
    return node;
}

static pcb* 
pcb_list_Get(pid_t pid)
{
    pcb* tmp = pcb_Head;

    while(tmp) {
        if(tmp->proc_pid == pid) {
            return tmp;
        }
        tmp = tmp->next;
    }

    return NULL;
}

static pcb*
pcb_list_Delete(pid_t pid)
{
    pcb* prev = NULL;
    pcb* curr = NULL;

    /* If the proc is at the starting position */
    if(pcb_Head->proc_pid == pid) {
        curr = pcb_Head;
        pcb_Head = pcb_Head->next;
        kfree(curr);
        return 0;
    }

    prev = pcb_Head;
    curr = prev->next;

    while(curr) {
        if(curr->proc_pid == pid) {
            prev->next = curr->next;
            kfree(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1;
}



static void
insertMinheap(pcb *proc, __INT32_TYPE__ value) 
{
    proc->curSize++;
    proc->heap[proc->curSize] = value;
    int32_t parent = (proc->curSize)/2;
    int32_t child = proc->curSize;

    while(parent > 0)
    {
        if(proc->heap[parent] > proc->heap[child])
        {
            int32_t temp = proc->heap[child];
            proc->heap[child] = proc->heap[parent];
            proc->heap[parent] = temp;

            child = parent;
            parent = parent / 2;
        }
        else
            break;
    }
}

static void
minHeapify(pcb *proc, int32_t index) 
{
    int32_t right = 2*index + 1;
    int32_t left = 2*index;

    int32_t smallest = index;

    if(right <= proc->curSize && proc->heap[right] < proc->heap[smallest])
        smallest = right;
    if(left <= proc->curSize && proc->heap[left] < proc->heap[smallest])
        smallest = left;

    if(smallest != index)
    {
        int temp = proc->heap[index];
        proc->heap[index] = proc->heap[smallest];
        proc->heap[smallest] = temp;

        minHeapify(proc, smallest);
    }
}

static int32_t 
extractMin(pcb *proc)
{
    int32_t min = proc->heap[1];
    proc->heap[1] = proc->heap[proc->curSize];
    proc->curSize--;
    minHeapify(proc, 1);
    return min;
}


static int
open(struct inode *inodep, struct file *filep)
{
    pid_t pid = current->pid;

    while(!mutex_trylock(&pcb_mutex));

    if(pcb_list_Get(pid)) {
        mutex_unlock(&pcb_mutex);
        printk(KERN_ERR "open() - File already Open in Process: %d\n", pid);
        return -EACCES;
    }

    pcb_list_Insert(pid);
    mutex_unlock(&pcb_mutex);

    printk(KERN_NOTICE "open() - File Opened by Process: %d\n", pid);

    return 0;
}


static ssize_t 
write(struct file *file, const char *buf, size_t count, loff_t *pos) 
{

    pid_t pid = current->pid;
    pcb* curr_proc = NULL;
    unsigned char* buffer = NULL;
    __INT32_TYPE__ value = 0;

    if(!buf || !count) {
        return -EINVAL;
    }

    while(!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if (curr_proc == NULL) {
        printk(KERN_ERR "write() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }

    buffer = (unsigned char*)kcalloc(MAX, sizeof(unsigned char), GFP_KERNEL);

    if(copy_from_user(buffer, buf, count)) {
        return -ENOBUFS;
    }

    if(buffer[0] == MIN_HEAP) {
        printk(KERN_INFO "MIN_HEAP TYPE \n");
        curr_proc->heapType = 0;
        curr_proc->heapSize = (int)buffer[1];
        if(curr_proc->heapSize < 1 || curr_proc->heapSize > 100) {
            printk(KERN_ERR "Invalid Size of Heap for Process %d", pid);
            return -1;
        }

        return count;
    }

    if(buffer[0] == MAX_HEAP) {
        printk(KERN_INFO "MAX_HEAP TYPE\n");
        curr_proc->heapType = 1;
        curr_proc->heapSize = (int)buffer[1];
        if(curr_proc->heapSize < 1 || curr_proc->heapSize > 100) {
            printk(KERN_ERR "Invalid Size of Heap for Process %d", pid);
            return -1;
        }
        return count;
    }


    value = *((__INT32_TYPE__*)buffer);

    if(curr_proc->curSize == curr_proc->heapSize) {
        printk(KERN_ERR "write() - Heap is FULL for Process %d", pid);
        return -1;
    }

    printk(KERN_INFO "Inserting %d in HEAP", value);

    
    if(curr_proc->heapType) {
        value = -value;
    }
    insertMinheap(curr_proc, value);


    kfree(buffer);
    
    return count;
}

static ssize_t
read(struct file *file, char *buf, size_t count, loff_t *pos)
{
    pid_t pid = current->pid;
    pcb* curr_proc = NULL;
    unsigned char* buffer = NULL;
    __INT32_TYPE__* value = NULL;
    size_t bytes = 0;

    if(!buf || !count) {
        return -EINVAL;
    }

    while(!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if(curr_proc == NULL) {
        printk(KERN_ERR "read() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }

    value = (__INT32_TYPE__*)kmalloc(sizeof(__INT32_TYPE__), GFP_KERNEL);

    if(curr_proc->curSize == 0) {
        printk(KERN_ERR "read() - Heap is EMPTY for Process: %d \n", pid);
        return -1;
    }

    *value = extractMin(curr_proc);

    if(curr_proc->heapType) {
        *value = -(*value);
    }

    buffer = (unsigned char*)value;
    bytes = sizeof(__INT32_TYPE__);

    if(copy_to_user(buf, buffer, count)) {
        return -ENOBUFS;
    }

    kfree(buffer);

    // printk(KERN_INFO "reading from kernel\n");
    
    return bytes;
}

static int 
release(struct inode *inodep, struct file *filep) 
{
    pid_t pid = current->pid;
    pcb* curr_proc = NULL;

    while (!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if (curr_proc == NULL) {
        printk(KERN_ERR "close() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }

    pcb_node_Reset(curr_proc);

    while (!mutex_trylock(&pcb_mutex));
    pcb_list_Delete(pid);
    mutex_unlock(&pcb_mutex);

    printk(KERN_NOTICE "close() - File Closed by Process: %d\n", pid);

    return 0;
}


int init_module()
{
    struct proc_dir_entry *entry = proc_create("partb_1_17CS10056_17CS30027", 0, NULL, &file_ops);
    if(!entry) {
        return -ENOENT;
    }


    file_ops.owner = THIS_MODULE;
    file_ops.write = write;
    file_ops.read = read;
    file_ops.open = open;
    file_ops.release = release;

    mutex_init(&pcb_mutex);
    printk(KERN_INFO "LKM initialized\n");

    return 0;
}

void cleanup_module()
{
    remove_proc_entry("partb_1_17CS10056_17CS30027", NULL);

    mutex_destroy(&pcb_mutex);
    printk(KERN_INFO "/proc/partb_1_17CS10056_17CS30027 removed\n");
}