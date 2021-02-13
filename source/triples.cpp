
// Вычисляет целочисленный хэш по sha1-хэшу
unsigned long get_mini_hash(unsigned char *hash) {
    unsigned long mini_hash = 0;
    for(int i=0; i<SHA_DIGEST_LENGTH; i++) {
        *(unsigned char*)((unsigned long)&mini_hash + (i % sizeof(unsigned long))) ^= hash[i];
    }
    return mini_hash;
}

// Вычисляет целочисленный хэш по произвольной строке
unsigned long get_mini_hash_char(char *value) {
    unsigned long mini_hash = 0;
    int j = strlen(value);
    char *invalue= (char*)malloc( j+1 );
    memcpy(invalue, value, j+1);
    int o = ( j > SHA_DIGEST_LENGTH ? j - SHA_DIGEST_LENGTH : 0 );
    int l = ( j > SHA_DIGEST_LENGTH ? SHA_DIGEST_LENGTH : j );
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)((unsigned long)invalue + o), l, hash);
    memcpy((unsigned char*)((unsigned long)invalue + o), hash, l);
    for(int i=0; i<j; i++) {
        *(unsigned char*)((unsigned long)&mini_hash + (i % sizeof(unsigned long))) ^= invalue[i];
    }
    free(invalue);
    return mini_hash;
}

// Внутренняя функция для бинарного поиска
long _find_using_index(mini_index *idx, unsigned long *n, unsigned long mini_hash, unsigned long *pos, unsigned long *chunk) {
    // Вернуть индекс элемента с хэшем mini_hash в массиве triples, иначе -1
    // Использовать массив индексов idx, в котором поле index содержит индекс элемента в массиве arr
    // В любом случае вернуть в pos позицию, в которую должен быть вставлен новый элемент
    *chunk = mini_hash >> (64 - chunk_bits);
    unsigned long offset = (*chunk) * CHUNK_SIZE;
    unsigned long l = offset;
    unsigned long r = offset + n[*chunk];
    unsigned long local_n = n[*chunk];
    *pos = offset;
    if (n == 0)
        return -1;
/* char message[1024];
sprintf(message, "Chunk = %lu, offset = %lu, size = %lu, bits = %i", *chunk, offset, local_n, chunk_bits);
logger(LOG, message, "", 0);
logger(LOG, "hash", "", idx[r-1].mini_hash); */
    if (n[*chunk] == 0) {
        *pos = (*chunk) * CHUNK_SIZE;
        return -1;
    }
    while (true) {
        // Цель этого цикла - найти либо равный элемент, либо такой, который больше, но перед ним стоит тот, который меньше
        unsigned long m = ((l + r) >> 1);
/* sprintf(message, "m = %lu, l = %lu, r = %lu, idx[m] = %020lu, %020lu", m, l, r, idx[m].mini_hash, mini_hash);
logger(LOG, message, "", 0); */
        // Если мы стоим у левой границы массива, то простая вилка: если первый элемент меньше, то перед ним, если больше, то после него
        if (m == offset) {
            if (idx[m].mini_hash > mini_hash) return -1;
            else if (idx[m].mini_hash == mini_hash) return offset;
            else { *pos = offset + 1; return -1; }
        }
        // Если мы у правой границы массива
        if (m == local_n - 1) {
            if (idx[m].mini_hash < mini_hash) { *pos = m+1; return -1; }
            else if (idx[m].mini_hash == mini_hash) return m;
            else if (idx[m-1].mini_hash == mini_hash) return m-1;
            else if(idx[m-1].mini_hash < mini_hash) { *pos = m; return -1; }
            else { *pos = m-1; return -1; }
        }
        // Здесь сразу рассматриваются два случая, 1) если текущий элемент равен искомому 2) если перед текущим элементом стоит элемент меньше, а сам он больше
        // В любом из этих случаев нам нужно либо вставить этот элемент на найденную позицию, либо обозначить что мы нашли равный
        if (idx[m].mini_hash == mini_hash) {
            *pos = m;
            return m;
        }
        if ( ( idx[m-1].mini_hash < mini_hash ) && ( mini_hash < idx[m].mini_hash ) ) { 
            *pos = m;
            return -1;
        }
        // Здесь уже простой сдвиг границ, т.к. мы ищем больший элемент, то если текущий меньше, то нужно искать в "правой", от текущей позиции, части массива
        if ( idx[m].mini_hash < mini_hash ) {
            if( l != m ) l = m;
            else {
                *pos = m;
                return -1;
            }
        }
        // Если больше, но не подошло ни под одно из вышеописанных условий, то ищем в "левой" части массива
        if ( idx[m].mini_hash > mini_hash ) {
            if( r != m )
                r = m;
            else {
                *pos = m;
                return -1;
            }
        }
    }
    return -1;
}

// Внешняя функция для поиска элемента, которая после бинарного поиска по целочисленному хэшу проверяет совпадение полного хэша
long find_using_index(mini_index *idx, unsigned long *n, unsigned char *hash, unsigned long mini_hash, unsigned long *pos, unsigned long *chunk) {
    sem_wait(sem);
    long ind = _find_using_index(idx, n, mini_hash, pos, chunk);
    sem_post(sem);
    if (ind != -1) {
        // Точно совпадающий элемент
        if (memcmp(triples[ idx[ ind ].index ].hash, hash, SHA_DIGEST_LENGTH) == 0) {
//logger(LOG, "Post-processing: element hash match", "", idx[ ind ].index);
            return ind;
        }
        // Если целочисленные хэши справа или слева совпадают - проверяем совпадение полных хэшей
        else {
//logger(LOG, "Post-processing: element hash NOT match", "", idx[ ind ].index);
            if (ind > 0) {
                while (ind > 0 && idx[ ind - 1 ].mini_hash == mini_hash) {
                    if (memcmp(triples[ idx[ ind ].index ].hash, hash, SHA_DIGEST_LENGTH) == 0)
                        return ind-1;
                    ind--;
                }
            }
            if (ind < n[*chunk]-1) {
                while (ind < n[*chunk]-1 && idx[ ind + 1 ].mini_hash == mini_hash) {
                    if (memcmp(triples[ idx[ ind ].index ].hash, hash, SHA_DIGEST_LENGTH) == 0)
                        return ind+1;
                    ind++;
                }
            }
            return -1;
        }
    }
    return ind;
}

unsigned long *find_matching_triples(char* subject, char* predicate, char* object, char *datatype, char *lang, unsigned long *n) {
        char data[MAX_VALUE_LEN * 5 + 128];
        unsigned char hash[SHA_DIGEST_LENGTH];
        long ind;
        unsigned long mini_hash, pos = 0, chunk = 0, size = 1024, *cand = (unsigned long*)malloc(sizeof(unsigned long*)*size);
        if(subject[0] != '*' && predicate[0] != '*' && object[0] != '*') {
            sprintf(data, "%s | %s | %s | %s | %s", subject, predicate, object, datatype, lang);
            memset(hash, 0, SHA_DIGEST_LENGTH);
            SHA1((unsigned char*)data, strlen(data), hash);
            mini_hash = get_mini_hash(hash);
            ind = find_using_index(full_index, chunks_size, hash, mini_hash, &pos, &chunk);
            if(ind == -1) return NULL;
            if(triples[full_index[ind].index].status != 0) return NULL;
            if((*n) >= size) { size += 1024; cand = (unsigned long *)realloc(cand, size); }
            cand[(*n)++] = full_index[ind].index;
        }
        else if(subject[0] != '*') {
            mini_hash = get_mini_hash_char(subject);
            ind = _find_using_index(s_index, chunks_size, mini_hash, &pos, &chunk);
            if (ind > 0) {
                while (ind > 0 && s_index[ ind-1 ].mini_hash == mini_hash) ind--;
                while (ind < (*n_triples) && s_index[ ind ].mini_hash == mini_hash) {
                    if(triples[ s_index[ ind ].index ].status != 0) { ind++; continue; }
                    char *s = get_string(triples[ s_index[ ind ].index ].s_pos);
                    if(strcmp(s, subject) != 0) { ind++; continue; }
                    int passed = 0;
                    if(predicate[0] == '*')
                        passed++;
                    else if(strcmp( predicate, get_string(triples[ s_index[ ind ].index ].p_pos)) == 0)
                        passed++;
                    if(object[0] == '*')
                        passed++;
                    else if(strcmp( object, get_string(triples[ s_index[ ind ].index ].o_pos)) == 0)
                        passed++;
                    if(passed == 2) {
                        if((*n) >= size) { size += 1024; cand = (unsigned long *)realloc(cand, size); }
                        cand[(*n)++] = s_index[ind].index;
                    }
                    ind++;
                }
            }
        }
        else if(object[0] != '*') {
            mini_hash = get_mini_hash_char(object);
            ind = _find_using_index(o_index, chunks_size, mini_hash, &pos, &size);
            if (ind > 0) {
                while (ind > 0 && o_index[ ind-1 ].mini_hash == mini_hash) ind--;
                while (ind < (*n_triples) && o_index[ ind ].mini_hash == mini_hash) {
                    if(triples[ o_index[ ind ].index ].status != 0) { ind++; continue; }
                    char *o = get_string(triples[ o_index[ ind ].index ].o_pos);
                    if(strcmp(o, object) != 0) { ind++; continue; }
                    int passed = 0;
                    if(predicate[0] == '*')
                        passed++;
                    else if(strcmp( predicate, get_string(triples[ o_index[ ind ].index ].p_pos)) == 0)
                        passed++;
                    if(subject[0] == '*')
                        passed++;
                    else if(strcmp( subject, get_string(triples[ o_index[ ind ].index ].s_pos)) == 0)
                        passed++;
                    if(passed == 2) {
                        if((*n) >= size) { size += 1024; cand = (unsigned long *)realloc(cand, size); }
                        cand[(*n)++] = o_index[ind].index;
                    }
                    ind++;
                }
            }
        }
        else if(predicate[0] != '*') {
            mini_hash = get_mini_hash_char(predicate);
            ind = _find_using_index(p_index, chunks_size, mini_hash, &pos, &chunk);
            if (ind > 0) {
                while (ind > 0 && p_index[ ind-1 ].mini_hash == mini_hash) ind--;
                while (ind < (*n_triples) && p_index[ ind ].mini_hash == mini_hash) {
                    if(triples[ p_index[ ind ].index ].status != 0) { ind++; continue; }
                    char *p = get_string(triples[ p_index[ ind ].index ].p_pos);
                    if(strcmp(p, predicate) != 0) continue;
                    int passed = 0;
                    if(subject[0] == '*')
                        passed++;
                    else if(strcmp( subject, get_string(triples[ p_index[ ind ].index ].s_pos)) == 0)
                        passed++;
                    if(object[0] == '*')
                        passed++;
                    else if(strcmp( object, get_string(triples[ p_index[ ind ].index ].o_pos)) == 0)
                        passed++;
                    if(passed == 2) {
                        if((*n) >= size) { size += 1024; cand = (unsigned long *)realloc(cand, size); }
                        cand[(*n)++] = p_index[ind].index;
                    }
                    ind++;
                }
            }
        }
        else {
            for(unsigned long i=0; i<(*n_triples); i++) {
                if((*n) >= size) { size += 1024; cand = (unsigned long *)realloc(cand, size); }
                if(triples[i].status != 0) continue;
                cand[(*n)++] = i;
            }
        }
        return cand;
}