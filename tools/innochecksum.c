/*
 ============================================================================
 Name        : innochecksum.c
 Author      : wentong
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h> /* for mmap and munmap */
#include <fcntl.h>     /* for open */
#include <assert.h>

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#define _XOPEN_SOURCE 500 /* needed to include getopt.h on some platforms. */

/* all of these ripped from InnoDB code from MySQL 4.0.22 */
#define UT_HASH_RANDOM_MASK     1463735687
#define UT_HASH_RANDOM_MASK2    1653893711
#define FIL_PAGE_LSN          16
#define FIL_PAGE_FILE_FLUSH_LSN 26
#define FIL_PAGE_OFFSET     4
#define FIL_PAGE_DATA       38
#define FIL_PAGE_END_LSN_OLD_CHKSUM 8
#define FIL_PAGE_SPACE_OR_CHKSUM 0
#define UNIV_PAGE_SIZE          (2 * 8192)

typedef unsigned long int ulint;
typedef unsigned char uchar;

/* innodb function in name; modified slightly to not have the ASM version (lots of #ifs that didn't apply) */
ulint mach_read_from_4(uchar *b) {
	return (((ulint) (b[0]) << 24) + ((ulint) (b[1]) << 16) + ((ulint) (b[2])
			<< 8) + (ulint) (b[3]));
}

ulint ut_fold_ulint_pair(
/*===============*/
/* out: folded value */
ulint n1, /* in: ulint */
ulint n2) /* in: ulint */
{
	return (((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1)
			^ UT_HASH_RANDOM_MASK) + n2);
}

ulint ut_fold_binary(
/*===========*/
/* out: folded value */
uchar* str, /* in: string of bytes */
ulint len) /* in: length */
{
	ulint i;
	ulint fold = 0;

	for (i = 0; i < len; i++) {
		fold = ut_fold_ulint_pair(fold, (ulint) (*str));

		str++;
	}

	return (fold);
}

ulint buf_calc_page_new_checksum(
/*=======================*/
/* out: checksum */
uchar* page) /* in: buffer page */
{
	ulint checksum;

	/* Since the fields FIL_PAGE_FILE_FLUSH_LSN and ..._ARCH_LOG_NO
	 are written outside the buffer pool to the first pages of data
	 files, we have to skip them in the page checksum calculation.
	 We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	 checksum is stored, and also the last 8 bytes of page because
	 there we store the old formula checksum. */

	checksum = ut_fold_binary(page + FIL_PAGE_OFFSET, FIL_PAGE_FILE_FLUSH_LSN
			- FIL_PAGE_OFFSET) + ut_fold_binary(page + FIL_PAGE_DATA,
			UNIV_PAGE_SIZE - FIL_PAGE_DATA - FIL_PAGE_END_LSN_OLD_CHKSUM);
	checksum = checksum & 0xFFFFFFFF;

	return (checksum);
}

ulint buf_calc_page_old_checksum(
/*=======================*/
/* out: checksum */
uchar* page) /* in: buffer page */
{
	ulint checksum;

	checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);

	checksum = checksum & 0xFFFFFFFF;

	return (checksum);
}


typedef struct tb_value {
	uchar *content;
	ulint page_no;
	int is_mmap;
} t_value;

struct Node {
	t_value* value;
	struct Node *next;
};

struct Queue {
	ulint length;
	pthread_mutex_t mutex;

	struct Node *head;
	struct Node *last;

};


t_value* create_value(uchar *content, ulint page_no) {
	t_value* value = (t_value*) malloc(sizeof(t_value));
	memset(value, 0, sizeof(t_value));
	value->content = content;
	value->page_no = page_no;
	return value;
}

struct Queue* init_queue() {
	struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
	memset(queue, 0, sizeof(struct Queue));
	pthread_mutex_init(&queue->mutex, NULL);
	return queue;
}

struct Node* create_node(t_value* value) {
	struct Node* node = (struct Node*) malloc(sizeof(struct Node));
	memset(node, 0, sizeof(struct Node));
	node->value = value;
	return node;

}

void put_into_queue(struct Queue *queue, t_value* value) {
	struct Node* node = create_node(value);
	if (queue->length == 0) {
		queue->head = node;
		queue->last = node;
		queue->length++;
	} else {
		queue->last->next = node;
		queue->last = node;
		queue->length++;
	}
}

t_value* pull_from_queue(struct Queue *queue) {
	if (queue->length == 0) {
		return NULL;
	} else {
		struct Node* head = queue->head;
		t_value* value = head->value;
		queue->head = head->next;
		free(head);
		queue->length--;
                assert(queue->length >= 0);
		if (queue->length == 0) {
			queue->last = NULL;
		}
		return value;
	}

}

static void** mem_pool;
/*
 * the count of mem pool
 */
static unsigned long int mem_pool_count;

static pthread_mutex_t mem_pool_mutex;

static unsigned long int block_size;

static unsigned long int mem_pool_size;

void init_mem_pool(unsigned long int block_s, unsigned long int mem_pool_s) {
    block_size = block_s;
    mem_pool_size = mem_pool_s;
    mem_pool_count = 0;
    pthread_mutex_init(&mem_pool_mutex, NULL);
    mem_pool = calloc(mem_pool_size, sizeof (void*));
}

void destory_mem_pool() {
    int i;
    pthread_mutex_destroy(&mem_pool_mutex);
    for (i = 0; i < mem_pool_count; i++) {
        free(mem_pool[i]);
    }
    free(mem_pool);
    mem_pool_count = 0;
    block_size = 0;
    mem_pool_size = 0;

}

void* get_from_pool() {
    void* ret;
    pthread_mutex_lock(&mem_pool_mutex);
    if (mem_pool_count > 0) {
        ret = mem_pool[--mem_pool_count];
    } else {
        ret = malloc(block_size);
    }
    pthread_mutex_unlock(&mem_pool_mutex);
    memset(ret, 0, block_size);
    return ret;
}

void put_to_pool(void* ptr) {
    pthread_mutex_lock(&mem_pool_mutex);
    if (mem_pool_count >= mem_pool_size) {
        free(ptr);
    } else {
        mem_pool[mem_pool_count++] = ptr;
    }
    pthread_mutex_unlock(&mem_pool_mutex);
}






#ifndef MAX_MEM_POOL_SIZE
#define MAX_MEM_POOL_SIZE 100
#endif


/* use mem is MAX_QUEUE_LENGTH*thread_num*UNIV_PAGE_SIZE */
#define MAX_QUEUE_LENGTH 1000000


#define inno_free(is_mmap,ptr) if (!is_mmap && ptr != NULL) {\
            put_to_pool(ptr);\
            ptr = NULL;\
        }

#define MAIN_RETURN(a) destory_mem_pool();return(a)

unsigned long long int check_file_size;

ulint total_page = 0;

int is_break = 0;

int is_read_end = 0;

int _max_queue_length = MAX_QUEUE_LENGTH;


static pthread_cond_t put_value_cond = PTHREAD_COND_INITIALIZER;

static pthread_cond_t get_value_cond = PTHREAD_COND_INITIALIZER;

typedef struct in_checksum_thread {
    struct Queue *queue;
    pthread_t *pid;
    int debug;
    /**1 is alive; 0 is dead**/
    int status;
    double num;

} checksum_thread_info;

typedef struct in_read_thread {
    struct Queue *queue;
    char *check_file_path;
    ulint start;
    ulint end;
} read_thread_info;

typedef struct in_monitor_thread {
    checksum_thread_info** thread_info_array;
    ulint thread_num;
} monitor_thread_info;

checksum_thread_info* create_thread_info(struct Queue* queue) {
    checksum_thread_info* ti = (checksum_thread_info*) malloc(sizeof (checksum_thread_info));
    memset(ti, 0, sizeof (checksum_thread_info));
    ti->queue = queue;
    ti->pid = malloc(sizeof (pthread_t));
    ti->status = 1;
    return ti;
}

void spin_loop_put(struct Queue* queue, t_value* value) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->length >= _max_queue_length) {
        pthread_cond_wait(&put_value_cond, &queue->mutex);
    }
    put_into_queue(queue, value);
    pthread_cond_broadcast(&get_value_cond);
    pthread_mutex_unlock(&queue->mutex);

    /*
        int i;
        while (1) {
            for (i = 0; i < _spin_loop; i++) {
                if (queue->length < _max_queue_length) {
                    put_into_queue(queue, value);
                    return;
                }
                put_spin_num++;
            }
            put_spin_wait++;
            usleep(_spin_delay);
        }
     */
}

t_value* spin_loop_get(struct Queue* queue) {
    t_value* value;
    struct timespec time_sp;
    memset(&time_sp, 0, sizeof (time_sp));
    while (1) {
        pthread_mutex_lock(&queue->mutex);
        value = pull_from_queue(queue);
        if (value != NULL) {
            pthread_cond_signal(&put_value_cond);
            pthread_mutex_unlock(&queue->mutex);
            return value;
        } else if (is_read_end) {
            pthread_mutex_unlock(&queue->mutex);
            return value;
        }
        time_sp.tv_sec = time(0) + 1;
        pthread_cond_timedwait(&get_value_cond, &queue->mutex, &time_sp);
        pthread_mutex_unlock(&queue->mutex);
    }

    /*
        int i;
        while (1) {
            for (i = 0; i < _spin_loop; i++) {
                t_value* value = pull_from_queue(queue);
                if (value != NULL) {
                    return value;
                } else if (is_read_end) {
                    return value;
                }
                get_spin_num++;
            }
            get_spin_wait++;
            usleep(_spin_delay);
        }
     */
}

void check_sum(checksum_thread_info* thread_info) {
    printf("start check_sum\n");
    int debug = thread_info->debug;
    uchar *p = NULL; /* storage of pages read */
    ulint ct; /* current page number (0 based) */
    ulint oldcsum, oldcsumfield, csum, csumfield, logseq, logseqfield; /* ulints for checksum storage */
    int is_mmap;
    t_value* value;
    while (!is_break) {

        value = spin_loop_get(thread_info->queue);
        if (value == NULL) {
            break;
        }
        p = value->content;
        ct = value->page_no;
        is_mmap = value->is_mmap;
	free(value);
        /* check the "stored log sequence numbers" */
        logseq = mach_read_from_4(p + FIL_PAGE_LSN + 4);
        logseqfield = mach_read_from_4(p + UNIV_PAGE_SIZE
                - FIL_PAGE_END_LSN_OLD_CHKSUM + 4);
        if (debug)
            printf(
                "page %lu: log sequence number: first = %lu; second = %lu\n",
                ct, logseq, logseqfield);
        if (logseq != logseqfield) {
            fprintf(stderr,
                    "page %lu invalid (fails log sequence number check)\n", ct);
            is_break = 1;
            break;
        }

        /* check old method of checksumming */
        oldcsum = buf_calc_page_old_checksum(p);
        oldcsumfield = mach_read_from_4(p + UNIV_PAGE_SIZE
                - FIL_PAGE_END_LSN_OLD_CHKSUM);
        if (debug)
            printf("page %lu: old style: calculated = %lu; recorded = %lu\n",
                ct, oldcsum, oldcsumfield);
        if (oldcsumfield != mach_read_from_4(p + FIL_PAGE_LSN) && oldcsumfield
                != oldcsum) {
            fprintf(stderr, "page %lu invalid (fails old style checksum)\n", ct);
            is_break = 1;
            break;
        }

        /* now check the new method */
        csum = buf_calc_page_new_checksum(p);
        csumfield = mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM);
        if (debug)
            printf("page %lu: new style: calculated = %lu; recorded = %lu\n",
                ct, csum, csumfield);
        if (csumfield != 0 && csum != csumfield) {
            fprintf(stderr, "page %lu invalid (fails new style checksum)\n", ct);
            is_break = 1;
            break;
        }
        inno_free(is_mmap, p)
        thread_info->num++;
    }
    inno_free(is_mmap, p)
    thread_info->status = 0;
}

void read_file_mmap(read_thread_info* info) {
    printf("start read_file_mmap\n");
    char *check_file_path = info->check_file_path;
    ulint start = info->start;
    ulint end = info->end;
    uchar *file;
    uchar *file_end;
    ulint ct; /* current page number (0 based) */
    off_t offset = 0;
    int fd;
    fd = open(check_file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    t_value *value;

    if (start) {
        offset = (off_t) (start) * (off_t) UNIV_PAGE_SIZE;
        if (offset >= check_file_size) {
            perror("unable to seek to necessary offset");
            is_break = 1;
            is_read_end = 1;
            return;
        }
    }
    file = mmap(NULL, check_file_size, PROT_READ, MAP_PRIVATE, fd, offset);
    madvise(file, check_file_size, MADV_RANDOM);
    file_end = file + check_file_size;
    ct = start;
    while (file < file_end - UNIV_PAGE_SIZE) {
        value = create_value(file, ct);
        value->is_mmap = 1;
        spin_loop_put(info->queue, value);
        /* end if this was the last page we were supposed to check */
        if (ct >= end) {
            is_read_end = 1;
            return;
        }
        ct++;
        file = file + UNIV_PAGE_SIZE;
    }
    is_break = 1;
    is_read_end = 1;
    //close(fd);
    //munmap(NULL, check_file_size);

}

void read_file_mmap_copy(read_thread_info* info) {
    printf("start read_file_mmap_copy\n");
    char *check_file_path = info->check_file_path;
    ulint start = info->start;
    ulint end = info->end;
    uchar *file;
    uchar *file_end;
    uchar *p;
    ulint ct; /* current page number (0 based) */
    off_t offset = 0;
    int fd;
    fd = open(check_file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    t_value *value;

    if (start) {
        offset = (off_t) (start) * (off_t) UNIV_PAGE_SIZE;
        if (offset >= check_file_size) {
            perror("unable to seek to necessary offset");
            is_break = 1;
            is_read_end = 1;
            close(fd);
            munmap(NULL, check_file_size);
            return;
        }
    }
    file = mmap(NULL, check_file_size, PROT_READ, MAP_PRIVATE, fd, offset);
    madvise(file, check_file_size, MADV_SEQUENTIAL);
    file_end = file + check_file_size;
    ct = start;
    while (file < file_end - UNIV_PAGE_SIZE) {
        p = (uchar *) get_from_pool();
        memcpy(p, file, UNIV_PAGE_SIZE);
        value = create_value(p, ct);
        spin_loop_put(info->queue, value);
        /* end if this was the last page we were supposed to check */
        if (ct >= end) {
            is_read_end = 1;
            close(fd);
            munmap(NULL, check_file_size);
            return;
        }
        ct++;
        file = file + UNIV_PAGE_SIZE;
    }
    is_break = 1;
    is_read_end = 1;
    //close(fd);
    //munmap(NULL, check_file_size);
}

void read_file(read_thread_info* info) {
    printf("start read_file\n");
    char *check_file_path = info->check_file_path;
    ulint start = info->start;
    ulint end = info->end;

    FILE *f; /* our input file */
    uchar *p; /* storage of pages read */
    int bytes; /* bytes read count */
    ulint ct; /* current page number (0 based) */
    off_t offset = 0;
    int fd;

    /* open the file for reading */
    f = fopen(check_file_path, "r");
    if (!f) {
        perror("error opening file");
        is_break = 1;
        is_read_end = 1;
        return;
    }

    /* seek to the necessary position */
    if (start) {
        fd = fileno(f);
        if (!fd) {
            perror("unable to obtain file descriptor number");
            is_break = 1;
            is_read_end = 1;
            return;
        }

        offset = (off_t) (start) * (off_t) UNIV_PAGE_SIZE;

        if (lseek(fd, offset, SEEK_SET) != offset) {
            perror("unable to seek to necessary offset");
            is_break = 1;
            is_read_end = 1;
            return;
        }
    }

    /* main checksumming loop */
    ct = start;
    while (!feof(f)) {
        if (is_break) {
            return;
        }
        p = (uchar *) get_from_pool();
        bytes = fread(p, 1, UNIV_PAGE_SIZE, f);
        if (!bytes && feof(f)) {
            is_read_end = 1;
            put_to_pool(p);
            return;
        }
        if (bytes != UNIV_PAGE_SIZE) {
            fprintf(stderr,
                    "bytes read (%d) doesn't match universal page size (%d)\n",
                    bytes, UNIV_PAGE_SIZE);
            is_break = 1;
            is_read_end = 1;
            put_to_pool(p);
            return;
        }

        spin_loop_put(info->queue, create_value(p, ct));
        /* end if this was the last page we were supposed to check */
        if (ct >= end) {
            is_read_end = 1;
            return;
        }

        ct++;
    }
    is_break = 1;
    is_read_end = 1;

}

int is_any_thread_alive(checksum_thread_info** info, ulint len) {
    int i;
    for (i = 0; i < len; i++) {
        if (info[i]->status == 1) {
            return 1;
        }
    }
    return 0;
}

float get_done(checksum_thread_info** info, ulint len) {
    float total = 0;
    int i;
    for (i = 0; i < len; i++) {
        total = total + info[i]->num;
    }
    return total;
}

void monitor(monitor_thread_info* info) {
    float total;
    printf("start monitor\n");
    while (is_any_thread_alive(info->thread_info_array, info->thread_num)) {
        sleep(1);
        total = get_done(info->thread_info_array, info->thread_num);
        printf("page %.0f okay: %.3f%% done\n", total, total / total_page * 100);
        //		fflush(stdout);
    }
}

int main(int argc, char **argv) {
    struct stat st; /* for stat, if you couldn't guess */
    ulint pages; /* number of pages in file */
    ulint start_page = 0, end_page = 0, use_end_page = 0; /* for starting and ending at certain pages */
    ulint thread_num = 1;
    int just_count = 0; /* if true, just print page count */
    int verbose = 0;
    int debug = 0;
    int c;
    char *check_file_path;
    struct Queue* queue;
    read_thread_info* r_thread_info;
    pthread_t read_pid;
    void* read_func;
    int ret;
    checksum_thread_info** thread_info_array;
    checksum_thread_info *check_thread_info;
    monitor_thread_info* m_info;
    pthread_t m_pid;

    /* remove arguments */
    while ((c = getopt(argc, argv, "cvds:e:p:t:r:m:")) != -1) {
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case 'c':
                just_count = 1;
                break;
            case 's':
                start_page = atoi(optarg);
                break;
            case 'e':
                end_page = atoi(optarg);
                use_end_page = 1;
                break;
            case 'p':
                start_page = atoi(optarg);
                end_page = atoi(optarg);
                use_end_page = 1;
                break;
            case 't':
                thread_num = atoi(optarg);
		if(thread_num < 1 || thread_num > 50){
		    fprintf(stderr, "option -t must > 0\n");
                    return 1;
		}
                break;
            case 'm':
                _max_queue_length = atoi(optarg);
                if (_max_queue_length < 1) {
                    fprintf(stderr, "option -m must >0\n");
                    return 1;
                }
                break;
            case 'd':
                debug = 1;
                break;
            case ':':
                fprintf(stderr, "option -%c requires an argument\n", optopt);
                return 1;
                break;
            case '?':
                fprintf(stderr, "unrecognized option: -%c\n", optopt);
                return 1;
                break;
        }
    }

    /* debug implies verbose... */
    if (debug)
        verbose = 1;

    /* make sure we have the right arguments */
    if (optind >= argc) {
        printf("InnoDB offline file checksum utility.\n");
        printf(
                "usage: %s [-c] [-s <start page>] [-e <end page>] [-p <page>] [-t <thread number>] [-v] [-d] <filename>\n",
                argv[0]);
        printf("\t-c\tprint the count of pages in the file\n");
        printf("\t-s n\tstart on this page number (0 based)\n");
        printf("\t-e n\tend at this page number (0 based)\n");
        printf("\t-p n\tcheck only this page (0 based)\n");
        printf("\t-v\tverbose (prints progress every 1 seconds)\n");
        printf("\t-d\tdebug mode (prints checksums for each page)\n");
        printf("\t-t n\tthe thread number\n");
        printf("\t-m n\tthe max_queue_length(1000000 based)\n");
        return 1;
    }
    check_file_path = argv[optind];
    /* stat the file to get size and page count */
    if (stat(check_file_path, &st)) {
        perror("error statting file");
        return 1;
    }
    check_file_size = st.st_size;
    pages = check_file_size / UNIV_PAGE_SIZE;
    total_page = pages;
    end_page = use_end_page ? end_page : (pages - 1);
    if (just_count) {
        printf("%lu\n", pages);
        return 0;
    } else if (verbose) {
        printf("file %s = %llu bytes (%lu pages)...\n", argv[optind], check_file_size,
                pages);
        printf("checking pages in range %lu to %lu\n", start_page, end_page);
    }

    //init
    init_mem_pool(UNIV_PAGE_SIZE,MAX_MEM_POOL_SIZE);
    queue = init_queue();

    //read_file thread_info
    r_thread_info = (read_thread_info*) malloc(sizeof (read_thread_info));
    memset(r_thread_info, 0, sizeof (read_thread_info));
    r_thread_info->check_file_path = check_file_path;
    r_thread_info->queue = queue;
    r_thread_info->start = start_page;
    r_thread_info->end = end_page;

    read_func = read_file;
    ret = pthread_create(&read_pid, NULL, (void*) read_func,
            r_thread_info);
    if (ret != 0) {
        printf("error");
        MAIN_RETURN(1);
    }

    //check_sum thread_info

    thread_info_array = (checksum_thread_info**) malloc(thread_num
            * sizeof (checksum_thread_info*));
    int i;
    for (i = 0; i < thread_num; i++) {
        check_thread_info = create_thread_info(queue);
        check_thread_info->debug = debug;
        thread_info_array[i] = check_thread_info;
        ret = pthread_create(check_thread_info->pid, NULL,
                (void*) check_sum, check_thread_info);
        if (ret != 0) {
            printf("error");
            MAIN_RETURN(1);
        }
    }

    //monitor thread_info
    m_info = (monitor_thread_info*) malloc(
            sizeof (monitor_thread_info));
    m_info->thread_info_array = thread_info_array;
    m_info->thread_num = thread_num;
    ret = pthread_create(&m_pid, NULL, (void*) monitor, m_info);
    if (ret != 0) {
        printf("error");
        MAIN_RETURN(1);
    }

    ret = pthread_join(read_pid, NULL);
    if (ret != 0) {
        printf("error");
        MAIN_RETURN(1);
    }

    for (i = 0; i < thread_num; i++) {
        ret = pthread_join(*(thread_info_array[i]->pid), NULL);
        if (ret != 0) {
            printf("error");
            MAIN_RETURN(1);
        }
    }

    ret = pthread_join(m_pid, NULL);
    if (ret != 0) {
        printf("error");
        MAIN_RETURN(1);
    }
    MAIN_RETURN(0);
}


