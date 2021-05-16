
class triples_index {

    public:

    // Get an integer hash by sha1-hash
    static unsigned long get_mini_hash(unsigned char *hash)
    {
        unsigned long mini_hash = 0;
        for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        {
            *(unsigned char *)((unsigned long)&mini_hash + (i % sizeof(unsigned long))) ^= hash[i];
        }
        return mini_hash;
    }

    // Get an integer hash by an arbitrary string
    static unsigned long get_mini_hash_char(char *value)
    {
        unsigned long mini_hash = 0;
        int j = strlen(value);
        char *invalue = (char *)malloc(j + 1);
        if(!invalue) utils::out_of_memory();
        memcpy(invalue, value, j + 1);
        int o = (j > SHA_DIGEST_LENGTH ? j - SHA_DIGEST_LENGTH : 0);
        int l = (j > SHA_DIGEST_LENGTH ? SHA_DIGEST_LENGTH : j);
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1((unsigned char *)((unsigned long)invalue + o), l, hash);
        memcpy((unsigned char *)((unsigned long)invalue + o), hash, l);
        for (int i = 0; i < j; i++)
        {
            *(unsigned char *)((unsigned long)&mini_hash + (i % sizeof(unsigned long))) ^= invalue[i];
        }
        free(invalue);
        return mini_hash;
    }

    // Pure binary search method
    // Return value is an index of the element with mini_hash hash in the *triples array, -1 if not found
    // Use an array of indexes *idx, in which an ->index field contains an index of the element in the indexed array
    // In any case returns in the *pos parameter the index position, in which a new element with mini_hash shall be inserted
    // *n is an array of chunks size, *chunk is a chunk index in *idx array (return parameter)
    static long _find_using_index(mini_index *idx, unsigned long *n, unsigned long mini_hash, unsigned long *pos, unsigned long *chunk)
    {
        *chunk = mini_hash >> (64 - chunk_bits);
        unsigned long offset = (*chunk) * (*single_chunk_size);
        unsigned long l = offset;
        unsigned long r = offset + n[*chunk];
        unsigned long local_n = n[*chunk];
        *pos = offset;
        if (n == 0)
            return -1;
    /*    char message[1024];
    utils::utils::logger(LOG, "", "", 0);
    sprintf(message, "Chunk = %lu, offset = %lu, size = %lu, bits = %i, hash = %020lu", *chunk, offset, local_n, chunk_bits, mini_hash);
    //printf("%s\n", message);
    utils::utils::logger(LOG, message, "", 0); */
        if (n[*chunk] == 0)
        {
            *pos = (*chunk) * (*single_chunk_size);
            return -1;
        }
        while (true)
        {
            // The goal of this cycle is to find an index element equal to mini_hash, or the pair of elements between which it shall be placed
            unsigned long m = ((l + r) >> 1);
/*          sprintf(message, "m = %lu, l = %lu, r = %lu, idx[m] = %020lu, mini_hash = %020lu (%lx, %lx)", m, l, r, idx[m].mini_hash, mini_hash, idx[m].mini_hash, mini_hash);
//          printf("%s\n", message);
            utils::utils::logger(LOG, message, "", 0); */
            // If we stay at the left array boundary: if the first array element is less that mini_hash - insert before it, or after it otherwise
            if (m == offset)
            {
                if (idx[m].mini_hash > mini_hash)
                    return -1;
                else if (idx[m].mini_hash == mini_hash)
                    return offset;
                else
                {
                    *pos = offset + 1;
                    return -1;
                }
            }
            // If we are at the right array boundary
            if (m == local_n - 1)
            {
                if (idx[m].mini_hash < mini_hash)
                {
                    *pos = m + 1;
                    return -1;
                }
                else if (idx[m].mini_hash == mini_hash) {
                    *pos = m + 1;
                    return m;
                }
                else if (idx[m - 1].mini_hash == mini_hash) {
                    *pos = m;
                    return m - 1;
                }
                else if (idx[m - 1].mini_hash < mini_hash)
                {
                    *pos = m;
                    return -1;
                }
                else
                {
                    *pos = m - 1;
                    return -1;
                }
            }
            // Two cases are considered here: 1) if the current element is equal to mini_hash, 2) if the element before current is less than mini_hash, and the current element is more than mini_hash
            // In these cases we have to insert an element to the current position, or return the found element
            if (idx[m].mini_hash == mini_hash)
            {
                *pos = m;
                return m;
            }
            if ((idx[m - 1].mini_hash < mini_hash) && (mini_hash < idx[m].mini_hash))
            {
                *pos = m;
                return -1;
            }
            // Move borders: if mini_hash is more that the current element - look in the right part of the index
            if (idx[m].mini_hash < mini_hash)
            {
                if (l != m)
                    l = m;
                else
                {
                    *pos = m + 1;
                    return -1;
                }
            }
            // Search in the left part otherwise
            if (idx[m].mini_hash > mini_hash)
            {
                if (r != m)
                    r = m;
                else
                {
                    *pos = m;
                    return -1;
                }
            }
        }
        return -1;
    }

    // An index search function for the external use. After performing binary search by integer hash it checks if the full hash matches
    static long find_using_index(mini_index *idx, unsigned long *n, unsigned char *hash, unsigned long mini_hash, unsigned long *pos, unsigned long *chunk, char *datatype, char *lang)
    {
        sem_wait(sem);
        long ind = _find_using_index(idx, n, mini_hash, pos, chunk);
        sem_post(sem);
        if (ind != -1)
        {
            while (ind > 0 && idx[ind - 1].mini_hash == mini_hash)
                ind--;
            while (ind < (*chunk)*(*single_chunk_size) + n[*chunk] && idx[ind].mini_hash == mini_hash)
            {
                // TODO: check equality of the strings, not only hashes
                if (memcmp(triples[idx[ind].index].hash, hash, SHA_DIGEST_LENGTH) == 0) {
                    if (datatype != NULL) {
                        if (triples[idx[ind].index].d_pos) {
                            char *dt = utils::get_string(triples[idx[ind].index].d_pos);
                            if (strcmp(dt, datatype) != 0) {
                                ind++;
                                continue;
                            }
                        }
                        else {
                            ind++;
                            continue;
                        }
                    }
                    if (lang != NULL) {
                        if (strcmp(triples[idx[ind].index].l, lang) != 0) {
                            ind++;
                            continue;
                        }
                    }
                    return ind;
                }
                ind++;
            }
            return -1;
        }
        return ind;
    }

    // Finds triples matching to the pattern passed. Returns an array of candidate triples and writes its size to *n parameter
    static unsigned long *find_matching_triples(char *subject, char *predicate, char *object, char *datatype, char *lang, unsigned long *n)
    {
        unsigned char hash[SHA_DIGEST_LENGTH];
        long ind;
//printf("search for %s - %s - %s\n", subject, predicate, object);
        unsigned long mini_hash, pos = 0, chunk = 0, size = 1024;
        unsigned long *cand = (unsigned long *)malloc(sizeof(unsigned long *) * size);
        if (!cand) utils::out_of_memory();
        if (subject[0] != '*' && predicate[0] != '*' && object[0] != '*')
        {
            int datatype_len = 0;
            if(datatype != NULL)
                datatype_len = strlen(datatype);
            char *data = (char*)malloc(strlen(subject) + strlen(predicate) + strlen(object) + datatype_len + 1024);
            if(!data) utils::out_of_memory();
            sprintf(data, "%s | %s | %s", subject, predicate, object);
            memset(hash, 0, SHA_DIGEST_LENGTH);
            SHA1((unsigned char *)data, strlen(data), hash);
            mini_hash = get_mini_hash(hash);
            free(data);
            ind = find_using_index(full_index, chunks_size, hash, mini_hash, &pos, &chunk, datatype, lang);
            if (ind == -1)
                return NULL;
            if (triples[full_index[ind].index].status != 0)
                return NULL;
            if ((*n) >= size - 1)
            {
                size += 1024;
                cand = (unsigned long *)realloc(cand, size * sizeof(unsigned long *));
            }
            cand[(*n)++] = full_index[ind].index;
        }
        else if (subject[0] != '*')
        {
            mini_hash = get_mini_hash_char(subject);
            ind = _find_using_index(s_index, s_chunks_size, mini_hash, &pos, &chunk);
            if (ind > -1)
            {
                while (ind > 0 && s_index[ind - 1].mini_hash == mini_hash)
                    ind--;
                while (ind < (*n_chunks) * (*single_chunk_size) && s_index[ind].mini_hash == mini_hash)
                {
                    if (triples[s_index[ind].index].status != 0)
                    {
                        ind++;
                        continue;
                    }
//                  if(!triples[s_index[ind].index].s_pos) { ind++; continue; }
                    char *s = utils::get_string(triples[s_index[ind].index].s_pos);
                    if(!s)
                    {
                        ind++;
                        continue;
                    }
                    if (strcmp(s, subject) != 0)
                    {
                        ind++;
                        continue;
                    }
                    int passed = 0;
                    if (predicate[0] == '*')
                        passed++;
                    else if (strcmp(predicate, utils::get_string(triples[s_index[ind].index].p_pos)) == 0)
                        passed++;
                    if (object[0] == '*')
                        passed++;
                    else if (strcmp(object, utils::get_string(triples[s_index[ind].index].o_pos)) == 0) {
                        if (datatype != NULL) {
                            if (strcmp(datatype, utils::get_string(triples[s_index[ind].index].d_pos)) != 0)
                                passed--;
                        }
                        if (lang != NULL) {
                            if (strcmp(lang, triples[s_index[ind].index].l) != 0)
                                passed--;
                        }
                        passed++;
                    }
                    if (passed == 2)
                    {
                        if ((*n) >= size - 1)
                        {
                            size += 1024;
                            cand = (unsigned long *)realloc(cand, size * sizeof(unsigned long *));
                        }
                        cand[(*n)++] = s_index[ind].index;
                    }
                    ind++;
                }
            }
        }
        else if (object[0] != '*')
        {
            mini_hash = get_mini_hash_char(object);
            ind = _find_using_index(o_index, o_chunks_size, mini_hash, &pos, &size);
            if (ind > -1)
            {
                while (ind > 0 && o_index[ind - 1].mini_hash == mini_hash)
                    ind--;
                while (ind < (*n_chunks) * (*single_chunk_size) && o_index[ind].mini_hash == mini_hash)
                {
                    if (triples[o_index[ind].index].status != 0)
                    {
                        ind++;
                        continue;
                    }
                    if(!triples[o_index[ind].index].o_pos) break;
                    char *o = utils::get_string(triples[o_index[ind].index].o_pos);
                    if(!o) break;
                    if (strcmp(o, object) != 0)
                    {
                        ind++;
                        continue;
                    }
                    int passed = 0;
                    if (predicate[0] == '*')
                        passed++;
                    else if (strcmp(predicate, utils::get_string(triples[o_index[ind].index].p_pos)) == 0)
                        passed++;
                    if (subject[0] == '*')
                        passed++;
                    else if (strcmp(subject, utils::get_string(triples[o_index[ind].index].s_pos)) == 0)
                        passed++;
                    if (passed == 2)
                    {
                        if ((*n) >= size - 1)
                        {
                            size += 1024;
                            cand = (unsigned long *)realloc(cand, size * sizeof(unsigned long *));
                        }
                        cand[(*n)++] = o_index[ind].index;
                    }
                    ind++;
                }
            }
        }
        else if (predicate[0] != '*')
        {
            mini_hash = get_mini_hash_char(predicate);
            ind = _find_using_index(p_index, p_chunks_size, mini_hash, &pos, &chunk);
            if (ind > -1)
            {
                while (ind > 0 && p_index[ind - 1].mini_hash == mini_hash)
                    ind--;
                while (ind < (*n_chunks) * (*single_chunk_size) && p_index[ind].mini_hash == mini_hash)
                {
                    if (triples[p_index[ind].index].status != 0)
                    {
                        ind++;
                        continue;
                    }
                    if(!triples[p_index[ind].index].p_pos) break;
                    char *p = utils::get_string(triples[p_index[ind].index].p_pos);
                    if(!p) break;
                    if (strcmp(p, predicate) != 0)
                        continue;
                    int passed = 0;
                    if (subject[0] == '*')
                        passed++;
                    else if (strcmp(subject, utils::get_string(triples[p_index[ind].index].s_pos)) == 0)
                        passed++;
                    if (object[0] == '*')
                        passed++;
                    else if (strcmp(object, utils::get_string(triples[p_index[ind].index].o_pos)) == 0) {
                        if (datatype != NULL) {
                            if (strcmp(datatype, utils::get_string(triples[s_index[ind].index].d_pos)) != 0)
                                passed--;
                        }
                        if (lang != NULL) {
                            if (strcmp(lang, triples[s_index[ind].index].l) != 0)
                                passed--;
                        }
                        passed++;
                    }
                    if (passed == 2)
                    {
                        if ((*n) >= size - 1)
                        {
                            size += 1024;
                            cand = (unsigned long *)realloc(cand, sizeof(unsigned long *) * size);
                        }
                        cand[(*n)++] = p_index[ind].index;
                    }
                    ind++;
                }
            }
        }
        else
        {
            for (unsigned long i = 0; i < (*n_triples); i++)
            {
                if (triples[i].status != 0)
                    continue;
                if ((*n) >= size - 1)
                {
                    size += 1024;
                    cand = (unsigned long *)realloc(cand, sizeof(unsigned long *) * size);
                }
                cand[(*n)++] = i;
            }
        }
        return cand;
    }

};