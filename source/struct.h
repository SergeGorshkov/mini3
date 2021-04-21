
#define OP_GET 1
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
#define VERSION "0.90"
#define STATUS_DELETED 1

#define N_MAX_PREFIX 64
#define N_MAX_VARIABLES 32
#define N_MAX_FILTERS 512
#define N_MAX_FILTER_GROUPS 32
#define N_MAX_ORDERS 32
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
#define B_OPTIONAL 6

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
#define COMPARE_EXISTS 9
#define COMPARE_NOTEXISTS 10
#define COMPARE_IEQUAL 11
#define COMPARE_INOTEQUAL 12
#define COMPARE_ICONTAINS 13
#define COMPARE_INOTCONTAINS 14

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
    int _union;
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
    int group;  // parent group
    int logic;  // and, or
};

struct chain_variable
{
    char obj[MAX_VALUE_LEN];
    int var;        // corresponding variable
    int dep;        // dependence score

    // Conditions - relations to another variables
    int n;          // number of conditions
    int cond_p[N_MAX_VARIABLES]; // reference to other variables (predicate)
    int cond_o[N_MAX_VARIABLES]; // reference to other variables (object)
    int cond_union[N_MAX_VARIABLES]; // contains the number of conditions group (UNION) or -1 if not in group
    bool notexists; // there is filter condition indicating that this variable must have no values
    bool optional;  // this variable may have no values
    int optional_group; // number of group of optional variables

    // Candidate objects (values)
    int n_cand;     // number of candidates
    char **cand;    // id of candidate of this variable's value
    char **cand_s;  // subject leading to this candidate
    char **cand_p;  // linkage (predicate or object) leading from subject to this candidate
    char **cand_d;  // value's datatype
    char **cand_l;  // value's lang

    // Dependent of variables
    int n_dep_of;
    int dependent_of[N_MAX_VARIABLES];

    // Solutions from this variable to dependent ones
    int *solution[N_MAX_VARIABLES]; // first index is the dependent variable index, value is its candidate object index
    int *solution_cand[N_MAX_VARIABLES]; // source candidate object index (of this variable's candidates)
    int *bearer[N_MAX_VARIABLES]; // for the predicate variables: bearer is the object which possess this predicate's value
    int n_sol[N_MAX_VARIABLES];  // number of solutions to each dependent variable

    // Combinations from this variable to dependent ones
    int n_comb;     // number of combinations
    int **comb_value; // index of the candidate object of that variable
};

unsigned long *global_block_ul = NULL;
prefix *prefixes = NULL;
triple *triples = NULL;
mini_index *full_index = NULL, *s_index = NULL, *p_index = NULL, *o_index = NULL;
unsigned long *active_requests = 0, *n_chunks = 0, *n_triples = 0, *n_prefixes = 0, *string_allocated = 0, *string_length = 0, *allocated = 0;
unsigned long *chunks_size = 0, *s_chunks_size = 0, *p_chunks_size = 0, *o_chunks_size = 0;
bool chunks_rebuilt = false, triples_reallocated = false;
int chunk_bits = 0;
char *stringtable = NULL;
sem_t *sem, *wsem, *rsem, *lsem, *psem;
int global_web_pip[4];
int sort_order = 0;

char database_path[1024];
char broker[1024], broker_host[1024], broker_port[1024], broker_user[1024], broker_password[1024], broker_queue[1024], broker_output_queue[1024];
bool nodaemon = false;

char *process_request(char *buffer, int operation, int endpoint, int modifier, int *pip);

