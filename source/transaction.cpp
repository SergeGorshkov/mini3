
class transaction {

    public:

    static bool write_to_pipe(int pip, int command, local_triple *t)
    {
        int nbytes = 0;
        do
        {
            ioctl(pip, FIONREAD, &nbytes);
            if (PIPESIZE - nbytes > 8192 + strlen(t->o))
                break;
            usleep(1000);
        } while (true);
        if (!write(pip, &command, sizeof(int)))
            return false;
        if (!write(pip, t, sizeof(indexable)))
            return false;
        int a = t->s ? strlen(t->s) : 0;
        if (!write(pip, &a, sizeof(int)))
            return false;
        if (a)
        {
            if (!write(pip, t->s, a))
                return false;
        }
        a = t->p ? strlen(t->p) : 0;
        if (!write(pip, &a, sizeof(int)))
            return false;
        if (a)
        {
            if (!write(pip, t->p, a))
                return false;
        }
        a = t->o ? strlen(t->o) : 0;
        if (!write(pip, &a, sizeof(int)))
            return false;
        if (a)
        {
            if (!write(pip, t->o, a))
                return false;
        }
        a = t->d ? strlen(t->d) : 0;
        if (!write(pip, &a, sizeof(int)))
            return false;
        if (a)
        {
            if (!write(pip, t->d, a))
                return false;
        }
        if (!write(pip, t->l, 8))
            return false;
        return true;
    }

    static void read_from_pipe(int pip, local_triple *t)
    {
        memset(t, 0, sizeof(triple));
        sem_wait(rsem);
        int a = 0;
        if (!read(pip, t, sizeof(indexable)))
        {
            sem_post(rsem);
            return;
        }
        read(pip, &a, sizeof(int));
        t->s = (char *)malloc(a + 1);
        if(!t->s) utils::out_of_memory();
        read(pip, t->s, a);
        t->s[a] = 0;
        read(pip, &a, sizeof(int));
        t->p = (char *)malloc(a + 1);
        if(!t->p) utils::out_of_memory();
        read(pip, t->p, a);
        t->p[a] = 0;
        read(pip, &a, sizeof(int));
        t->o = (char *)malloc(a + 1);
        if(!t->o) utils::out_of_memory();
        read(pip, t->o, a);
        t->o[a] = 0;
        read(pip, &a, sizeof(int));
        if (a) {
            t->d = (char *)malloc(a + 1);
            if(!t->d) utils::out_of_memory();
            read(pip, t->d, a);
            t->d[a] = 0;
        }
        else
            t->d = NULL;
        read(pip, t->l, 8);
        sem_post(rsem);
    }

    // Expands array of triples if necessary
    static void realloc_triples(int mode)
    {
        //utils::logger(LOG, "Start realloc", "", (*allocated)*sizeof(triple));
        sem_wait(sem);
        munmap(triples, (*allocated) * sizeof(triple));
        triples = NULL;
        char fname[1024];
        strcpy(fname, database_path);
        strcat(fname, DATABASE_FILE);
        int fd = open(fname, O_RDWR);
        ftruncate(fd, ((*allocated) + SEGMENT) * sizeof(triple));
        close(fd);
        (*allocated) += SEGMENT;
        sem_post(sem);
        triples_reallocated = true;
        //utils::logger(LOG, "End realloc", "", 0);
        utils::init_globals(mode);
    }

    static mini_index *rebuild_chunk_index(char *file, mini_index *index, unsigned long *size)
    {
        unsigned long newsize[MAX_CHUNKS];
        memset(newsize, 0, sizeof(unsigned long) * MAX_CHUNKS);
        mini_index *tmp = (mini_index *)malloc((*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
        if(!tmp) utils::out_of_memory();
        memcpy(tmp, index, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
        munmap(index, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
        mini_index *newindex = (mini_index *)utils::mmap_file(O_RDWR, file, (*n_chunks) * 2 * CHUNK_SIZE * sizeof(mini_index));
        /*
char logstr[1024];
utils::logger(LOG, "Dumping index before re-chunk ", file, (*n_chunks)*CHUNK_SIZE*sizeof(mini_index));
for(unsigned long c=0; c<*n_chunks; c++) {
    if(size[c] == 0) continue;
    sprintf(logstr, "Chunk %lu, %lu items", c, size[c]);
    utils::logger(LOG, logstr, "", 0);
    //continue;
    for(int i=0; i<size[c]; i++) {
        sprintf(logstr, "%lx\t%lu", tmp[c*CHUNK_SIZE + i].mini_hash, tmp[c*CHUNK_SIZE + i].index);
        utils::logger(LOG, logstr, "", 0);
    }
}
utils::logger(LOG, "\n", "", 0); */
        for (int i = 0; i < (*n_chunks); i++)
        {
            bool moved = false;
            for (int j = 0; j < size[i]; j++)
            {
                unsigned long target = tmp[i * CHUNK_SIZE + j].mini_hash >> (64 - chunk_bits - 1);
                // The records behind this point are going to the second part of the new chunk
                if (target == i * 2 + 1)
                {
                    if (j > 0)
                    {
                        memcpy((void *)((unsigned long)newindex + (i * 2 * CHUNK_SIZE) * sizeof(mini_index)), (void *)((unsigned long)tmp + i * CHUNK_SIZE * sizeof(mini_index)), j * sizeof(mini_index));
                        newsize[i * 2] = j;
                    }
                    if (j < size[i] - 1)
                    {
                        memcpy((void *)((unsigned long)newindex + ((i * 2 + 1) * CHUNK_SIZE) * sizeof(mini_index)), (void *)((unsigned long)tmp + (i * CHUNK_SIZE + j) * sizeof(mini_index)), (size[i] - j) * sizeof(mini_index));
                        newsize[i * 2 + 1] = size[i] - j;
                    }
                    moved = true;
                    break;
                }
            }
            // If there is no boundary in this chunk
            if (!moved && size[i] > 0)
            {
                memcpy((void *)((unsigned long)newindex + (i * 2 * CHUNK_SIZE) * sizeof(mini_index)), (void *)((unsigned long)tmp + i * CHUNK_SIZE * sizeof(mini_index)), size[i] * sizeof(mini_index));
                newsize[i * 2] = size[i];
            }
        }
        free(tmp);
        memcpy(size, newsize, sizeof(unsigned long) * MAX_CHUNKS);
        /* utils::logger(LOG, "Dumping index after re-chunk ", file, (*n_chunks)*2*CHUNK_SIZE*sizeof(mini_index));
for(unsigned long c=0; c<(*n_chunks)*2; c++) {
    if(size[c] == 0) continue;
    sprintf(logstr, "Chunk %lu, %lu items", c, size[c]);
    utils::logger(LOG, logstr, "", 0);
    //continue;
    for(int i=0; i<size[c]; i++) {
        sprintf(logstr, "%lx\t%lu", newindex[c*CHUNK_SIZE + i].mini_hash, newindex[c*CHUNK_SIZE + i].index);
        utils::logger(LOG, logstr, "", 0);
    }
}
utils::logger(LOG, "\n", "", 0); */
        return newindex;
    }

    static bool check_chunk_size(mini_index *idx, unsigned long size, unsigned long n)
    {
    //utils::logger(LOG, "Check chunk size", "", size);
        if (size != CHUNK_SIZE)
            return false;
        unsigned long offset = n * CHUNK_SIZE;
        for (unsigned long i = 1; i < CHUNK_SIZE; i++)
        {
            if (idx[offset + i].mini_hash != idx[offset].mini_hash)
                return true;
        }
    //utils::logger(LOG, "Check chunk size", "return false", 0);
        return false;
    }

    static void rebuild_chunks(void)
    {
        chunks_rebuilt = true;
        utils::logger(LOG, "Repartitioning indexes", "", (*n_chunks));
        if ((*n_chunks) == MAX_CHUNKS)
            utils::logger(ERROR, "Max amount of chunks is reached, cannot scale, exiting", "", 0);
        full_index = rebuild_chunk_index(INDEX_FILE, full_index, chunks_size);
        s_index = rebuild_chunk_index(S_INDEX_FILE, s_index, s_chunks_size);
        p_index = rebuild_chunk_index(P_INDEX_FILE, p_index, p_chunks_size);
        o_index = rebuild_chunk_index(O_INDEX_FILE, o_index, o_chunks_size);
        *n_chunks = (*n_chunks) * 2;
        utils::logger(LOG, "Set number of chunks ", "", (*n_chunks));
        chunk_bits++;
        for (int i = 0; i < (*n_chunks); i++)
        {
            if (check_chunk_size(full_index, chunks_size[i], i) || check_chunk_size(s_index, s_chunks_size[i], i) || check_chunk_size(p_index, p_chunks_size[i], i) || check_chunk_size(o_index, o_chunks_size[i], i))
            {
                rebuild_chunks();
                break;
            }
        }
    }

    static unsigned long insert_into_index(char *s, mini_index *index, unsigned long *f_chunks_size)
    {
        unsigned long target_chunk = 0, pos = 0;
        unsigned long mini_hash = triples_index::get_mini_hash_char(s);
        triples_index::_find_using_index(index, f_chunks_size, mini_hash, &pos, &target_chunk);
    /* char strl[1024];
sprintf(strl, "%lx", mini_hash);
utils::logger(LOG, "insert_into_index", s, pos);
utils::logger(LOG, strl, "chunk", target_chunk); */
        // If there are many similar identifiers in S, P or O index, a chunk may overflow. In this case we use a neighboring chunk
        if (f_chunks_size[target_chunk] == CHUNK_SIZE)
        {
            bool found = false;
            for (unsigned long i = target_chunk + 1; i < (*n_chunks); i++)
            {
                if (f_chunks_size[i] < CHUNK_SIZE)
                {
                    target_chunk = i;
                    pos = 0;
                    found = true;
                    break;
                }
            }
            if (!found)
                for (unsigned long i = target_chunk - 1; i >= 0; i--)
                {
                    if (f_chunks_size[i] < CHUNK_SIZE)
                    {
                        target_chunk = i;
                        pos = f_chunks_size[i];
                        found = true;
                        break;
                    }
                }
            if (!found)
                utils::logger(ERROR, "Cannot update index, chunks are overflown", s, 0);
        }
        if (target_chunk * CHUNK_SIZE + f_chunks_size[target_chunk] - pos > 0)
            memmove((void *)((unsigned long)index + (pos + 1) * sizeof(mini_index)),
                    (void *)((unsigned long)index + pos * sizeof(mini_index)),
                    sizeof(mini_index) * (f_chunks_size[target_chunk] + target_chunk * CHUNK_SIZE - pos));
        f_chunks_size[target_chunk]++;
        index[pos].index = *n_triples;
        index[pos].mini_hash = mini_hash;
        /*
char str[1024];
sprintf(str, "Insert: pos = %lu, chunk = %i, %i triples in chunk\n", pos, target_chunk, f_chunks_size[target_chunk]);
utils::logger(LOG, str, "", 0); */
        return target_chunk;
    }

    static char *global_commit_triple(local_triple *t)
    {
        unsigned long pos = 0;
        char message[10240];
        /*
sprintf(message, "%lu", t->mini_hash);
utils::logger(LOG, "(parent) Global commit", message, *n_triples);
    */
        unsigned long target_chunk = 0;
        long ind = triples_index::find_using_index(full_index, chunks_size, t->hash, t->mini_hash, &pos, &target_chunk, t->d, t->l);
    /*
sprintf(message,"(parent) Triple %s with hash %lx has ind %i, pos %lu in chunk %lu", t->o, t->mini_hash, ind, pos, target_chunk);
utils::logger(LOG, message, "", 0);
    */
        // If this triple already exists - skip it
        if (ind > -1)
        {
            // Restore triple if it is deleted
            if (triples[full_index[ind].index].status == STATUS_DELETED)
            {
                sem_wait(sem);
                triples[full_index[ind].index].status = 0;
                sem_post(sem);
            }
            return NULL;
        }
//utils::logger(LOG, "(parent) Waiting for semaphore", "", getpid());
        sem_wait(sem);
//sprintf(message, "%lu of %lu", *n_triples, *allocated);
//utils::logger(LOG, "(parent) Continue", message, getpid());
        if (*n_triples >= *allocated)
        {
            sem_post(sem);
            realloc_triples(O_RDWR);
            sem_wait(sem);
        }
        memcpy(&triples[*n_triples], t, sizeof(indexable));
        memcpy(&triples[*n_triples].l, t->l, 8);
        triples[*n_triples].s_len = strlen(t->s);
        triples[*n_triples].s_pos = utils::add_string(t->s, *n_triples);
        triples[*n_triples].p_len = strlen(t->p);
        triples[*n_triples].p_pos = utils::add_string(t->p, *n_triples);
        triples[*n_triples].o_len = strlen(t->o);
        triples[*n_triples].o_pos = utils::add_string(t->o, *n_triples);
        if(t->d) {
            triples[*n_triples].d_len = strlen(t->d);
            triples[*n_triples].d_pos = utils::add_string(t->d, *n_triples);
        }
        else {
            triples[*n_triples].d_len = 0;
            triples[*n_triples].d_pos = 0;
        }

    //sprintf(message,"insert into main index: chunk = %lu, offset = %lu, pos = %lu\n", target_chunk, target_chunk * CHUNK_SIZE + chunks_size[target_chunk], pos);
    //utils::logger(LOG, message, "", 0);
        if (target_chunk * CHUNK_SIZE + chunks_size[target_chunk] - pos > 0)
            memmove((void *)((unsigned long)full_index + (pos + 1) * sizeof(mini_index)),
                    (void *)((unsigned long)full_index + pos * sizeof(mini_index)),
                    (target_chunk * CHUNK_SIZE + chunks_size[target_chunk] - pos) * sizeof(mini_index));
        chunks_size[target_chunk]++;
        full_index[pos].index = *n_triples;
        full_index[pos].mini_hash = t->mini_hash;
        // S-index
        unsigned long s_target_chunk = insert_into_index(t->s, s_index, s_chunks_size);
        // P-index
        unsigned long p_target_chunk = insert_into_index(t->p, p_index, p_chunks_size);
        // O-index
        unsigned long o_target_chunk = insert_into_index(t->o, o_index, o_chunks_size);
        (*n_triples)++;
        if (chunks_size[target_chunk] == CHUNK_SIZE || s_chunks_size[s_target_chunk] == CHUNK_SIZE || p_chunks_size[p_target_chunk] == CHUNK_SIZE || o_chunks_size[o_target_chunk] == CHUNK_SIZE)
            rebuild_chunks();
        sem_post(sem);
        save_globals();
//utils::logger(LOG, "Insert done", "", 0);
        return NULL;
    }

    static void save_globals(void)
    {
        sem_wait(sem);
        *(unsigned long *)global_block_ul = *n_triples;
        *(unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long)) = *n_prefixes;
        *(unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 2) = 0;
        *(unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 3) = *allocated;
        *(unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 4) = *string_allocated;
        *(unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 5) = *string_length;
        *(unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 6) = *n_chunks;
        sem_post(sem);
    }

    static char *global_delete_triple(unsigned long pos)
    {
        char message[1024];
        if (pos < 0 || pos >= (*n_triples))
            return NULL;
        sem_wait(sem);
        triples[pos].status = STATUS_DELETED;
        sem_post(sem);
        return NULL;
    }

};