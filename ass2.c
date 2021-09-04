/*
Name - Ayush Tiwari
Roll No - 17CS10056

Name - Ritikesh Gupta
Roll No - 17CS30027

Kernel Version - 5.2.6

For running testcase filename = partb_2_17CS10056_17CS30027

*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define MAX 101
#define INF 100000

#define PB2_SET_TYPE _IOW(0x10, 0x31, int32_t*)
#define PB2_INSERT _IOW(0x10, 0x32, int32_t*)
#define PB2_GET_INFO _IOR(0x10, 0x33, int32_t*)
#define PB2_EXTRACT _IOR(0x10, 0x34, int32_t*)


typedef struct pb2_set_type_arguments { 
    int32_t heap_size;      // size of the heap
    int32_t heap_type;      // heap type: 0 for min-heap, 1 for max-heap
}type_args;

typedef struct obj_info_ {
    int32_t heap_size;      // current number of elements in heap (current size of the heap)
    int32_t heap_type;      // heap type: 0 for min-heap, 1 for max-heap
    int32_t root;           // value of the root node of the heap (null if size is 0).
    int32_t last_inserted;  // value of the last element inserted in the heap.
}obj_info;

typedef struct result_ {
    int32_t element;        // value of min/max element extracted.
    int32_t heap_size;      // current size of the heap after extracting.
}result;

typedef struct pcb_ {
    pid_t proc_pid;         // ProcessID of the owner process
    type_args type;         // Stores the type and size of heap
    int32_t last;           // Stores the last inserted element in the heap
    int32_t cur_size;       // Dynamic size of the heap
    int32_t heap[101];      // Stores the elements of heap
    struct pcb_* next;      // Stores link to next Block in the  list
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
    proc->type.heap_type = -1;
    proc->type.heap_size = 0;
    proc->cur_size = 0;
    proc->last = 0;
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
    proc->type.heap_type = -1;
    proc->type.heap_size = 0;
    proc->cur_size = 0;
    int i;
    for(i = 0 ; i <= 100 ; i++) {
        proc->heap[i] = 0;
    }
    proc->last = 0;

    return;
}

static pcb* 
pcb_list_Insert(pid_t pid) 
{
    pcb* proc = pcb_node_Create(pid);

    proc->next = pcb_Head;
    pcb_Head = proc;
 
    return proc;
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


obj_info* obj_info_Create(int32_t sz,
                          int32_t tp,
                          int32_t rt,
                          int32_t last)
{
    obj_info *obj = (obj_info*)kmalloc(sizeof(obj_info), GFP_KERNEL);

    obj->heap_size = sz;
    obj->heap_type = tp;
    obj->root = rt;
    obj->last_inserted = last;

    return obj;
}


static void
insertMinheap(pcb *proc, __INT32_TYPE__ value) 
{
    proc->cur_size++;
    proc->heap[proc->cur_size] = value;
    proc->last = value;
    int32_t parent = (proc->cur_size)/2;
    int32_t child = proc->cur_size;

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

    if(right <= proc->cur_size && proc->heap[right] < proc->heap[smallest])
        smallest = right;
    if(left <= proc->cur_size && proc->heap[left] < proc->heap[smallest])
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
    proc->heap[1] = proc->heap[proc->cur_size];
    proc->heap[proc->cur_size] = 0;
    proc->cur_size--;
    minHeapify(proc, 1);
    return min;
}

static ssize_t 
Min_Heap_Info(pcb* proc, obj_info* info)
{
    info->heap_size = proc->cur_size;
    info->heap_type = proc->type.heap_type;
    info->root = proc->heap[1];
    info->last_inserted = proc->last;

    return sizeof(obj_info);
}

static ssize_t
MIN_HEAP_RESULT(pcb* proc, result* res) 
{
    res->element = extractMin(proc);
    res->heap_size = proc->cur_size;
    return sizeof(result);
}


static long 
ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    pid_t pid = current->pid;
    pcb* curr_proc = NULL;
    obj_info* info = NULL;
    result* res = NULL;
    type_args* type = NULL;
    __INT8_TYPE__ error = 0;
    __INT32_TYPE__* value = NULL;
    long return_value = 0;

    while (!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if (curr_proc == NULL) {
        printk(KERN_ERR "ioctl() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }

    value = (__INT32_TYPE__*)kmalloc(sizeof(__INT32_TYPE__), GFP_KERNEL);
    res = (result*)kmalloc(sizeof(result), GFP_KERNEL);
    type = (type_args*)kmalloc(sizeof(type_args), GFP_KERNEL);
    info = obj_info_Create(0, -1, 0, INF);

    switch (cmd) {
        case PB2_SET_TYPE:  
                            if (copy_from_user(type, (type_args*)arg, sizeof(type_args))) {
                                error = -ENOBUFS;
                                break;
                            }
                            if(type->heap_size < 1 || type->heap_size > 100) {
                                printk(KERN_ERR "ioctl() - Process: %d Invalid Size of Heap\n", pid);
                                error = -EINVAL;
                                break;
                            }

                            pcb_node_Reset(curr_proc);

                            switch (type->heap_type) {
                                case 0:  printk(KERN_NOTICE "ioctl() - Process: %d Set Heap Type to MIN_HEAP\n", pid);
                                                curr_proc->type.heap_type = 0;
                                                curr_proc->type.heap_size = type->heap_size;
                                                break;

                                case 1:  printk(KERN_NOTICE "ioctl() - Process: %d Set Heap Type to MAX_HEAP\n", pid);
                                                curr_proc->type.heap_type = 1;
                                                curr_proc->type.heap_size = type->heap_size;
                                                break;

                                default:        printk(KERN_ALERT "ioctl() - Unrecognized Heap Type in Process: %d\n", pid);
                                                error = -EINVAL;
                                                break;
                            }
                            break;
                            
        case PB2_INSERT: 
                            if (curr_proc->type.heap_type == -1) {
                                printk(KERN_ALERT "ioctl() - PB2_INSERT not called by Process: %d\n", pid);
                                error = -EACCES;
                                break;
                            }

                            if (curr_proc->cur_size == curr_proc->type.heap_size) {
                                printk(KERN_ERR "ioctl() - PB2_INSERT Heap is FULLL");
                                error = -1;
                                break;
                            }

                            if (copy_from_user(value, (__INT32_TYPE__*)arg, sizeof(__INT32_TYPE__))) {
                                error = -ENOBUFS;
                                break;
                            }

                            if(curr_proc->type.heap_type == 0) {
                                
                                printk(KERN_INFO "Inserting %d in MIN HEAP", *value);
                                insertMinheap(curr_proc, *value);
                            }

                            if(curr_proc->type.heap_type == 1) {
                                
                                printk(KERN_INFO "Inserting %d in MAX HEAP", *value);
                                *value = -(*value);
                                insertMinheap(curr_proc, *value);   
                            }

                            return_value = sizeof(*value);
                            break;

        case PB2_GET_INFO:  
                            if (curr_proc->type.heap_type == -1) {
                                printk(KERN_ALERT "ioctl() - PB2_GET_INFO not called by Process: %d\n", pid);
                                error = -EACCES;
                                break;
                            }

                            if(curr_proc->type.heap_type == 0) {
                                
                                printk(KERN_ALERT "ioctl() - Process: %d Getting MIN HEAP Info\n", pid);
                                return_value = Min_Heap_Info(curr_proc, info);
                            }

                            if(curr_proc->type.heap_type == 1) {
                                
                                printk(KERN_ALERT "ioctl() - Process: %d Getting MAX HEAP Info\n", pid);
                                return_value = Min_Heap_Info(curr_proc, info);
                                info->root = -(info->root);
                                info->last_inserted = -(info->last_inserted);
                            }

                            if (copy_to_user((obj_info*)arg, info, sizeof(obj_info))) {
                                error = -ENOBUFS;
                                break;
                            }

                            break;
                        
        case PB2_EXTRACT:   
                            if (curr_proc->type.heap_type == -1) {
                                printk(KERN_ALERT "ioctl() - PB2_EXTRACT not called by Process: %d\n", pid);
                                error = -EACCES;
                                break;
                            }

                            if (curr_proc->cur_size == 0) {
                                printk(KERN_ERR "ioctl() - PB2_INSERT Heap is EMPTY");
                                error = -1;
                                break;
                            }

                            if(curr_proc->type.heap_type == 0) {
                                
                                printk(KERN_ALERT "ioctl() - Process: %d Getting MIN HEAP element\n", pid);
                                return_value = MIN_HEAP_RESULT(curr_proc, res);                            
                            }

                            if(curr_proc->type.heap_type == 1) {
                                
                                printk(KERN_ALERT "ioctl() - Process: %d Getting MAX HEAP element\n", pid);
                                return_value = MIN_HEAP_RESULT(curr_proc, res);                                
                                res->element = -(res->element);
                            }

                            if (copy_to_user((result*)arg, res, sizeof(result))) {
                                error = -ENOBUFS;
                                break;
                            }
                            break;
                            
        default:            printk(KERN_ALERT "ioctl() - Invalid Command in Process: %d\n", pid);
                            error = -EINVAL;
                            break;
    }
    kfree(value);
    kfree(type);
    kfree(info);
    kfree(res);

    if (error < 0) {
        return error;
    }

    return 0;
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
    struct proc_dir_entry *entry = proc_create("partb_2_17CS10056_17CS30027", 0, NULL, &file_ops);
    if(!entry) {
        return -ENOENT;
    }

    file_ops.owner = THIS_MODULE;
    file_ops.open = open;
    file_ops.release = release;
    file_ops.unlocked_ioctl = ioctl;

    mutex_init(&pcb_mutex);
    printk(KERN_INFO "LKM initialized\n");

    return 0;
}

void cleanup_module()
{
    remove_proc_entry("partb_2_17CS10056_17CS30027", NULL);

    mutex_destroy(&pcb_mutex);
    printk(KERN_INFO "/proc/partb_2_17CS10056_17CS30027 removed\n");
}