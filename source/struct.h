
#define OP_GET 1
#define OP_POST 2
#define OP_PUT 3
#define OP_DELETE 4

#define EP_PREFIX 1
#define EP_TRIPLE 2
#define EP_CHAIN 3

#define MOD_COUNT 1

#define MAX_JSON_TOKENS 16384
#define MAX_TAG_LEN 256
#define MAX_VALUE_LEN 1024 // do not increase
#define BUFSIZE 8096
#define PIPESIZE 16384
#define ERROR 42
#define LOG 44
#define FORBIDDEN 403
#define NOTFOUND 404
#define VERSION 1
#define STATUS_DELETED 1

#define N_MAX_PREFIX 64
#define SEGMENT 32768    // The amount of triples after which the storage is reallocated
#define CHUNK_SIZE 1024  // The segment size after which the index is chunked further
#define MAX_CHUNKS 16384 // The maximal possible amount of chunks
#define N_GLOBAL_VARS 7

#define COMMIT_TRIPLE 1
#define DELETE_TRIPLE 2

#define RESULT_ERROR 1
#define RESULT_OK 2
#define RESULT_CHUNKS_REBUILT 4
#define RESULT_TRIPLES_REALLOCATED 8

#define B_PATTERN 1
#define B_CHAIN 2
#define B_PREFIX 3
#define B_ORDER 4
#define B_FILTER 5

#define LOGIC_AND 0
#define LOGIC_OR 1

#define COMPARE_EQUAL 1
#define COMPARE_NOTEQUAL 2
#define COMPARE_CONTAINS 3
#define COMPARE_NOTCONTAINS 4
#define COMPARE_MORE 5
#define COMPARE_MOREOREQUAL 6
#define COMPARE_LESS 7
#define COMPARE_LESSOREQUAL 8

#define PREFIXES_FILE "prefixes.bin"
#define STAT_FILE "stat.bin"
#define CHUNKS_FILE "chunks.bin"
#define S_CHUNKS_FILE "s_chunks.bin"
#define P_CHUNKS_FILE "p_chunks.bin"
#define O_CHUNKS_FILE "o_chunks.bin"
#define DATABASE_FILE "database.bin"
#define INDEX_FILE "index.bin"
#define S_INDEX_FILE "s_index.bin"
#define P_INDEX_FILE "p_index.bin"
#define O_INDEX_FILE "o_index.bin"
#define STRING_FILE "data.bin"

struct indexable
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned long mini_hash;
    int status;
};

struct local_triple : indexable
{
    struct indexable;
    char *s;
    char *p;
    char *o;
    char *d;
    char l[8];
};

struct triple : indexable
{
    struct indexable;
    unsigned long s_pos;
    unsigned long s_len;
    unsigned long p_pos;
    unsigned long p_len;
    unsigned long o_pos;
    unsigned long o_len;
    unsigned long d_pos;
    unsigned long d_len;
    char l[8];
};

struct mini_index
{
    unsigned long index;
    unsigned long mini_hash;
};

struct prefix
{
    char shortcut[MAX_VALUE_LEN];
    char value[MAX_VALUE_LEN];
    int len;
};

struct order
{
    char variable[MAX_VALUE_LEN];
    char direction[MAX_VALUE_LEN];
};

struct filter
{
    int group;
    char variable[MAX_VALUE_LEN];
    int operation;
    char value[MAX_VALUE_LEN];
};

struct filter_group
{
    int group;
    int logic;
};

struct chain_variable
{
    char obj[MAX_VALUE_LEN];
    int var;        // corresponding variable
    int dep;        // dependence score

    int n;          // number of conditions
    int cond_p[32]; // reference to other variables (predicate)
    int cond_o[32]; // reference to other variables (object)

    int n_cand;     // number of candidates
    char **cand;    // id of candidate of this variable's value
    char **cand_s;  // subject leading to this candidate
    char **cand_p;  // linkage (predicate or object) leading from subject to this candidate
    char **cand_d;  // value's datatype
    char **cand_l;  // value's lang
};

unsigned long *global_block_ul = NULL;
prefix *prefixes = NULL;
triple *triples = NULL;
mini_index *full_index = NULL, *s_index = NULL, *p_index = NULL, *o_index = NULL;
unsigned long *n_chunks = 0, *n_triples = 0, *n_prefixes = 0, *string_allocated = 0, *string_length = 0, *allocated = 0, *db_version = 0;
unsigned long *chunks_size = 0, *s_chunks_size = 0, *p_chunks_size = 0, *o_chunks_size = 0;
bool chunks_rebuilt = false, triples_reallocated = false;
int chunk_bits = 0;
char *stringtable = NULL;
sem_t *sem, *wsem, *rsem, *lsem, *psem;
int global_web_pip[4];
int sort_order = 0;

char database_path[1024];
char broker[1024], broker_host[1024], broker_port[1024], broker_user[1024], broker_password[1024], broker_queue[1024], broker_output_queue[1024];

void logger(int type, char *s1, char *s2, int socket_fd);
char *process_request(char *buffer, int operation, int endpoint, int modifier, int *pip);
void save_globals(void);
long find_using_index(mini_index *idx, unsigned long *n, unsigned char *hash, unsigned long mini_hash, unsigned long *pos, unsigned long *chunk);
unsigned long *find_matching_triples(char *subject, char *predicate, char *object, char *datatype, char *lang, unsigned long *n);
bool write_to_pipe(int pip, int command, local_triple *t);
void read_from_pipe(int pip, local_triple *t);
char *get_string(unsigned long pos);
unsigned long add_string(char *s, unsigned long n);
void out_of_memory(void);
void out_of_memory(int fd);

void *mmap_file(int mode, char *file, size_t size)
{
    char fname[1024];
    int fd, mapmode = (mode == O_RDWR ? PROT_READ | PROT_WRITE : PROT_READ);
    if (size == 0)
        size = 1024;
    strcpy(fname, database_path);
    strcat(fname, file);
    // Increase file size as required
    fd = open(fname, O_RDWR | O_CREAT);
    chmod(fname, 0666);
    ftruncate(fd, size);
    close(fd);
    fd = open(fname, mode);
    if (!fd)
        logger(ERROR, "cannot open file", file, errno);
    void *block = mmap((caddr_t)0, size, mapmode, MAP_SHARED, fd, 0);
    close(fd);
    if (block == MAP_FAILED)
        logger(ERROR, "mmap failed", file, errno);
    return block;
}

void init_globals(int mode)
{
    sem_wait(sem);
    if (!global_block_ul)
        global_block_ul = (unsigned long *)mmap_file(O_RDWR, STAT_FILE, sizeof(unsigned long) * N_GLOBAL_VARS);
    if (!chunks_size)
    {
        chunks_size = (unsigned long *)mmap_file(mode, CHUNKS_FILE, sizeof(unsigned long) * MAX_CHUNKS);
        s_chunks_size = (unsigned long *)mmap_file(mode, S_CHUNKS_FILE, sizeof(unsigned long) * MAX_CHUNKS);
        p_chunks_size = (unsigned long *)mmap_file(mode, P_CHUNKS_FILE, sizeof(unsigned long) * MAX_CHUNKS);
        o_chunks_size = (unsigned long *)mmap_file(mode, O_CHUNKS_FILE, sizeof(unsigned long) * MAX_CHUNKS);
    }
    n_triples = (unsigned long *)global_block_ul;
    n_prefixes = (unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long));
    db_version = (unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 2);
    allocated = (unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 3);
    string_allocated = (unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 4);
    string_length = (unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 5);
    n_chunks = (unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 6);
    if (*n_chunks == 0)
        *n_chunks = 4;
    chunk_bits = log2(*n_chunks);

    if (*allocated == 0)
        (*allocated) = SEGMENT;
    if (!prefixes)
        prefixes = (prefix *)mmap_file(O_RDWR, PREFIXES_FILE, N_MAX_PREFIX * sizeof(prefix));
    if (!triples)
        triples = (triple *)mmap_file(mode, DATABASE_FILE, (*allocated) * sizeof(triple));
    if (!full_index)
        full_index = (mini_index *)mmap_file(mode, INDEX_FILE, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
    if (!s_index)
        s_index = (mini_index *)mmap_file(mode, S_INDEX_FILE, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
    if (!p_index)
        p_index = (mini_index *)mmap_file(mode, P_INDEX_FILE, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
    if (!o_index)
        o_index = (mini_index *)mmap_file(mode, O_INDEX_FILE, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
    if (!stringtable)
        stringtable = (char *)mmap_file(mode, STRING_FILE, (*string_allocated));
    sem_post(sem);
}
