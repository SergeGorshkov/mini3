/*
    This file is part of mini-3 triple store.

    Mini-3 triple store is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Mini-3 triple store is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Mini-3 triple store.  If not, see <https://www.gnu.org/licenses/>.
*/

class utils {

    public:

    static void logger(int type, char *s1, char *s2, int socket_fd)
    {
        int fd;
        char buffer[1024];
        char *logbuffer = (char *)malloc(strlen(s1) + strlen(s2) + 1024);
        if(!logbuffer) out_of_memory(socket_fd);

        switch (type)
        {
        case ERROR:
            (void)sprintf(logbuffer, "503 ERROR: %s: %s Errno=%d exiting pid=%d", s1, s2, errno, getpid());
            break;
        case FORBIDDEN:
            (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 184\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 270);
            (void)sprintf(logbuffer, "403 FORBIDDEN: %s: %s", s1, s2);
            break;
        case NOTFOUND:
            (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 135\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n", 223);
            (void)sprintf(logbuffer, "404 NOT FOUND: %s: %s", s1, s2);
            break;
        case LOG:
            (void)sprintf(logbuffer, " %s ", s1);
            if (s2[0] != 0)
                strcat(logbuffer, s2);
            if (socket_fd != 0)
                sprintf((char *)((long)logbuffer + strlen(logbuffer)), " = %i", socket_fd);
        }
        #ifndef WEBSERVER
        printf("%s\n", logbuffer);
        if (type == ERROR)
        {
            int len = strlen(logbuffer);
            (void)sprintf(buffer, "HTTP/1.1 503 %s\nServer: mini3/%d.0\nConnection: close\nContent-Type: text/plain\n\n", logbuffer, VERSION); // Header + a blank line
            (void)write(socket_fd, buffer, strlen(buffer));
        }
        #endif
        time_t rt;
        time(&rt);
        tm *t = localtime(&rt);
        strftime(buffer, 80, "%F %T\t", t);
        sem_wait(lsem);
        if(nodaemon)
            printf("%s\n%s\n", buffer, logbuffer);
        else {
            if ((fd = open("mini-3.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0)
            {
                (void)write(fd, buffer, strlen(buffer));
                (void)write(fd, logbuffer, strlen(logbuffer));
                (void)write(fd, "\n", 1);
                close(fd);
            }
        }
        if(nodaemon) {
        printf("%s %s\n", buffer, logbuffer);
        }
        sem_post(lsem);
        free(logbuffer);
        if ((type == ERROR || type == NOTFOUND || type == FORBIDDEN) && socket_fd > 0) {
            close(socket_fd);
            exit(1);
        }
    }

    static char *file_get_contents(char *filename)
    {
        int file_fd, len;
        char *buffer;
        if ((file_fd = open(filename, O_RDONLY)) == -1)
        {
            utils::logger(NOTFOUND, "failed to open file", filename, 0);
            exit;
        }
        len = (long)lseek(file_fd, (off_t)0, SEEK_END);
        (void)lseek(file_fd, (off_t)0, SEEK_SET);
        buffer = (char *)malloc(len + 1);
        if(!buffer) out_of_memory();
        read(file_fd, buffer, len);
        close(file_fd);
        buffer[len] = 0;
        return buffer;
    }

    static void unprefix(char *id_full, char *id_short)
    {
        const char *short_prefixes[] = {"owl", "rdfs", "rdf", "archigraph", "xsd"};
        const char *full_prefixes[] = {"http://www.w3.org/2002/07/owl#", "http://www.w3.org/2000/01/rdf-schema#", "http://www.w3.org/1999/02/22-rdf-syntax-ns#", "http://trinidata.ru/archigraph-mdm/", "http://www.w3.org/2001/XMLSchema#"};

        for (int i = 0; i < 5; i++)
        {
            if (strncmp(id_full, short_prefixes[i], strlen(short_prefixes[i])) == 0)
            {
                strcpy(id_short, (char *)((long)id_full + strlen(short_prefixes[i]) + 1));
                strcpy(id_full, full_prefixes[i]);
                strcat(id_full, id_short);
            }
        }
        char *spos = strrchr(id_full, (int)'/');
        char *rpos = strrchr(id_full, (int)'#');
        if (rpos > spos)
            spos = rpos;
        strcpy(id_short, (char *)((long)spos + 1));
    }

    static void strtolower(char *s)
    {
        int len = strlen(s);
        char *lower[33] = {"а", "б", "в", "г", "д", "е", "ё", "ж", "з", "и", "й", "к", "л", "м", "н", "о", "п", "р", "с", "т", "у", "ф", "х", "ц", "ч", "ш", "щ", "ъ", "ы", "ь", "э", "ю", "я"};
        char *upper[33] = {"А", "Б", "В", "Г", "Д", "Е", "Ё", "Ж", "З", "И", "Й", "К", "Л", "М", "Н", "О", "П", "Р", "С", "Т", "У", "Ф", "Х", "Ц", "Ч", "Ш", "Щ", "Ъ", "Ы", "Ь", "Э", "Ю", "Я"};
        for (int i = 0; i < len; i++)
        {
            s[i] = tolower(s[i]);
            for (int j = 0; j < 33; j++)
            {
                int ll = strlen(lower[j]);
                int ul = strlen(upper[j]);
                if (strncmp((char *)((long)s + i), upper[j], 2) == 0)
                {
                    if (ll == ul)
                    {
                        memcpy((char *)((long)s + i), lower[j], ll);
                        break;
                    }
                }
            }
        }
    }

    static void descreen(char *s)
    {
        int len = strlen(s), newi = 0;
        char *newstr = (char *)malloc(len + 1);
        if(!newstr) out_of_memory();
        for (int i = 0; i < len; i++)
        {
            if (s[i] == '\\' && s[i + 1] == '/')
            {
                newstr[newi++] = '/';
                i++;
            }
            else
                newstr[newi++] = s[i];
        }
        newstr[newi] = 0;
        strcpy(s, newstr);
        free(newstr);
    }

    static void replacedot(char *s, bool direction)
    {
        int len = strlen(s), newi = 0;
        char *newstr = (char *)malloc(len + 1024);
        if(!newstr) out_of_memory();
        for (int i = 0; i < len; i++)
        {
            if (direction)
            {
                if (s[i] == '.')
                {
                    newstr[newi++] = '(';
                    newstr[newi++] = '_';
                    newstr[newi++] = 'd';
                    newstr[newi++] = 'o';
                    newstr[newi++] = 't';
                    newstr[newi++] = '_';
                    newstr[newi++] = ')';
                }
                else
                    newstr[newi++] = s[i];
            }
            else
            { // back to dots
                if (strncmp((char *)((long)s + i), "(_dot_)", 7) == 0)
                {
                    newstr[newi++] = '.';
                    i += 6;
                }
                else
                    newstr[newi++] = s[i];
            }
        }
        newstr[newi] = 0;
        strcpy(s, newstr);
        free(newstr);
    }

    static char *str_replace(const char *s, const char *oldW, const char *newW)
    {
        char *result;
        int i, cnt = 0;
        int newWlen = strlen(newW);
        int oldWlen = strlen(oldW);

        for (i = 0; s[i] != '\0'; i++)
        {
            if (strstr(&s[i], oldW) == &s[i])
            {
                cnt++;
                i += oldWlen - 1;
            }
        }

        result = (char *)malloc(i + cnt * (newWlen - oldWlen) + 1);
        if(!result) out_of_memory();

        i = 0;
        while (*s)
        {
            if (strstr(s, oldW) == s)
            {
                strcpy(&result[i], newW);
                i += newWlen;
                s += oldWlen;
            }
            else
                result[i++] = *s++;
        }

        result[i] = '\0';
        return result;
    }

    static void unicode_to_utf8(char *buffer)
    {
        int tp = 0, r = strlen(buffer);
        char *t = (char *)malloc(r + 1);
        if(!t) out_of_memory();
        for (int i = 0; i < r; i++)
        {
            if (buffer[i] == '\\' && buffer[i + 1] == 'u')
            {
                char ch1[5], ch2[5];
                int cd1, cd2, cd;
                ch1[0] = '0';
                ch1[1] = 'x';
                ch1[2] = buffer[i + 2];
                ch1[3] = buffer[i + 3];
                ch1[4] = 0;
                ch2[0] = '0';
                ch2[1] = 'x';
                ch2[2] = buffer[i + 4];
                ch2[3] = buffer[i + 5];
                ch2[4] = 0;
                sscanf(ch1, "%x", &cd1);
                sscanf(ch2, "%x", &cd2);
                cd = (cd1 << 8) + cd2;
                if (cd < 0x80)
                    t[tp++] = (char)cd;
                else if (cd < 0x800)
                {
                    t[tp++] = 192 + cd / 64;
                    t[tp++] = 128 + cd % 64;
                }
                else if (cd < 0x10000)
                {
                    t[tp++] = 224 + cd / 4096;
                    t[tp++] = 128 + cd / 64 % 64;
                    t[tp++] = 128 + cd % 64;
                }
                else if (cd < 0x110000)
                {
                    t[tp++] = 240 + cd / 262144;
                    t[tp++] = 128 + cd / 4096 % 64;
                    t[tp++] = 128 + cd / 64 % 64;
                    t[tp++] = 128 + cd % 64;
                }
                i += 5;
            }
            else
                t[tp++] = buffer[i];
        }
        t[tp] = 0;
        strcpy(buffer, t);
        free(t);
    }

    static int json_to_array(char *json, char **arr)
    {
        bool started = false;
        if (json == NULL)
            return 0;
        int start = 0, n = 0, l = strlen(json);
        for (int i = 0; i < l; i++)
        {
            if (json[i] == '"' && json[i - 1] != '\\')
            {
                if (started)
                {
                    arr[n] = (char *)malloc(i - start + 1);
                    if(!arr[n]) out_of_memory();
                    memcpy(arr[n], (void *)((long)json + start + 1), i - start - 1);
                    arr[n][i - start - 1] = 0;
                    n++;
                    started = false;
                }
                else
                {
                    started = true;
                    start = i;
                }
            }
        }
        return n;
    }

    static const unsigned char base64_table[65];

    /**
     * base64_encode - Base64 encode
     * @src: Data to be encoded
     * @len: Length of the data to be encoded
     * @out_len: Pointer to output length variable, or %NULL if not used
     * Returns: Allocated buffer of out_len bytes of encoded data,
     * or %NULL on failure
     *
     * Caller is responsible for freeing the returned buffer. Returned buffer is
     * nul terminated to make it easier to use as a C string. The nul terminator is
     * not included in out_len.
     */
    static unsigned char *base64_encode(const unsigned char *src, size_t len, size_t *out_len)
    {
        unsigned char *out, *pos;
        const unsigned char *end, *in;
        size_t olen;
        int line_len;

        olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
        olen += olen / 72;      /* line feeds */
        olen++;                 /* nul termination */
        if (olen < len)
            return NULL; /* integer overflow */
        out = (unsigned char *)malloc(olen);
        if(!out) out_of_memory();

        end = src + len;
        in = src;
        pos = out;
        line_len = 0;
        while (end - in >= 3)
        {
            *pos++ = base64_table[in[0] >> 2];
            *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
            *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
            *pos++ = base64_table[in[2] & 0x3f];
            in += 3;
            line_len += 4;
            if (line_len >= 72)
            {
                *pos++ = '\n';
                line_len = 0;
            }
        }

        if (end - in)
        {
            *pos++ = base64_table[in[0] >> 2];
            if (end - in == 1)
            {
                *pos++ = base64_table[(in[0] & 0x03) << 4];
                *pos++ = '=';
            }
            else
            {
                *pos++ = base64_table[((in[0] & 0x03) << 4) |
                                    (in[1] >> 4)];
                *pos++ = base64_table[(in[1] & 0x0f) << 2];
            }
            *pos++ = '=';
            line_len += 4;
        }

        if (line_len)
            *pos++ = '\n';

        *pos = '\0';
        if (out_len)
            *out_len = pos - out;
        return out;
    }

    /**
     * base64_decode - Base64 decode
     * @src: Data to be decoded
     * @len: Length of the data to be decoded
     * @out_len: Pointer to output length variable
     * Returns: Allocated buffer of out_len bytes of decoded data,
     * or %NULL on failure
     *
     * Caller is responsible for freeing the returned buffer.
     */
    static unsigned char *base64_decode(const unsigned char *src, size_t len, size_t *out_len)
    {
        unsigned char dtable[256], *out, *pos, block[4], tmp;
        size_t i, count, olen;
        int pad = 0;

        memset(dtable, 0x80, 256);
        for (i = 0; i < sizeof(base64_table) - 1; i++)
            dtable[base64_table[i]] = (unsigned char)i;
        dtable['='] = 0;

        count = 0;
        for (i = 0; i < len; i++)
        {
            if (dtable[src[i]] != 0x80)
                count++;
        }

        if (count == 0 || count % 4)
            return NULL;

        olen = count / 4 * 3;
        pos = out = (unsigned char *)malloc(olen);
        if(!out) out_of_memory();

        count = 0;
        for (i = 0; i < len; i++)
        {
            tmp = dtable[src[i]];
            if (tmp == 0x80)
                continue;

            if (src[i] == '=')
                pad++;
            block[count] = tmp;
            count++;
            if (count == 4)
            {
                *pos++ = (block[0] << 2) | (block[1] >> 4);
                *pos++ = (block[1] << 4) | (block[2] >> 2);
                *pos++ = (block[2] << 6) | block[3];
                count = 0;
                if (pad)
                {
                    if (pad == 1)
                        pos--;
                    else if (pad == 2)
                        pos -= 2;
                    else
                    {
                        /* Invalid padding */
                        free(out);
                        return NULL;
                    }
                    break;
                }
            }
        }

        *out_len = pos - out;
        return out;
    }

    static int cstring_cmp(const void *a, const void *b) {
        const char **ia = (const char **)a;
        const char **ib = (const char **)b;
        int res = strcmp(*ia, *ib);
        return sort_order ? -res : res;
    }

    static void out_of_memory(void) {
        static char s[] = "Out of memory\n";
        int fd;
        if ((fd = open("mini3.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0)
        {
            (void)write(fd, s, strlen(s));
            (void)write(fd, "\n", 1);
            close(fd);
        }
        exit(1);
    }

    static void out_of_memory(int fd) {
        (void)write(fd, "HTTP/1.1 500 Internal server error\nContent-Length: 104\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nOut of memory\n</body></html>\n", 191);
        close(fd);
        out_of_memory();
    }

    static char *get_string(unsigned long pos)
    {
        return (char *)((unsigned long)stringtable + pos + sizeof(unsigned long) * 2);
    }

    static void *mmap_file(int mode, char *file, size_t size)
    {
        char fname[1024];
        int fd, mapmode = (mode == O_RDWR ? PROT_READ | PROT_WRITE : PROT_READ);
        if (size == 0)
            size = 1024;
        strcpy(fname, database_path);
        strcat(fname, file);
        DIR *dir = opendir(database_path);
        if(dir)
            closedir(dir);
        else {
            if(mkdir(database_path, 0777) == -1) {
                printf("Cannot open database directory %s: it does not exists or not enough access rights\n", database_path);
                utils::logger(ERROR, "Cannot open database directory: it does not exists or not enough access rights", database_path, 0);
            }
        }
        // Increase file size as required
        fd = open(fname, O_RDWR | O_CREAT);
        chmod(fname, 0666);
        ftruncate(fd, size);
        close(fd);
        fd = open(fname, mode);
        if (!fd)
            utils::logger(ERROR, "cannot open file", file, errno);
        void *block = mmap((caddr_t)0, size, mapmode, MAP_SHARED, fd, 0);
        close(fd);
        if (block == MAP_FAILED)
            utils::logger(ERROR, "mmap failed", file, errno);
        return block;
    }

    static void init_globals(int mode)
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
        single_chunk_size = (unsigned long *)((unsigned long)global_block_ul + sizeof(unsigned long) * 2);
        if(*single_chunk_size < INITIAL_CHUNK_SIZE) *single_chunk_size = INITIAL_CHUNK_SIZE;
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
            full_index = (mini_index *)mmap_file(mode, INDEX_FILE, (*n_chunks) * (*single_chunk_size) * sizeof(mini_index));
        if (!s_index)
            s_index = (mini_index *)mmap_file(mode, S_INDEX_FILE, (*n_chunks) * (*single_chunk_size) * sizeof(mini_index));
        if (!p_index)
            p_index = (mini_index *)mmap_file(mode, P_INDEX_FILE, (*n_chunks) * (*single_chunk_size) * sizeof(mini_index));
        if (!o_index)
            o_index = (mini_index *)mmap_file(mode, O_INDEX_FILE, (*n_chunks) * (*single_chunk_size) * sizeof(mini_index));
        if (!stringtable)
            stringtable = (char *)mmap_file(mode, STRING_FILE, (*string_allocated));
        sem_post(sem);
    }

    // s is a string, n is an index of triple to which it belongs
    static unsigned long add_string(char *s, unsigned long n)
    {
        unsigned long len = strlen(s), pos = (*string_length);
        if (len == 0)
            return 0;
        if ((*string_length) + len + sizeof(unsigned long) * 2 + 1 > (*string_allocated))
        {
            munmap(stringtable, *string_allocated);
            char fname[1024];
            strcpy(fname, database_path);
            strcat(fname, STRING_FILE);
            int fd = open(fname, O_RDWR);
            (*string_allocated) += len + sizeof(unsigned long) * 2 + 1 < getpagesize() ? getpagesize() : len + getpagesize();
            ftruncate(fd, *string_allocated);
            stringtable = (char *)mmap((caddr_t)0, (*string_allocated), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            close(fd);
        }
        memcpy((void *)((unsigned long)stringtable + (*string_length)), &len, sizeof(unsigned long));
        memcpy((void *)((unsigned long)stringtable + (*string_length) + sizeof(unsigned long)), &n, sizeof(unsigned long));
        strcpy((char *)((unsigned long)stringtable + (*string_length) + sizeof(unsigned long) * 2), s);
        (*string_length) += len + sizeof(unsigned long) * 2 + 1;
        return pos;
    }

    static void load_config(char *message)
    {
        jsmn_parser parser;
        jsmntok_t tokens[MAX_JSON_TOKENS];
        int r, status;
        char t[1024], prev_t[1024], cause[32];
        t[0] = 0;
        prev_t[0] = 0;
        broker[0] = 0;
        broker_host[0] = 0;
        broker_port[0] = 0;
        broker_user[0] = 0;
        broker_password[0] = 0;
        broker_queue[0] = 0;
        char *rbuffer = utils::file_get_contents("config.json");
        jsmn_init(&parser);
        memset(&tokens, 0, MAX_JSON_TOKENS * sizeof(jsmntok_t));
        r = jsmn_parse(&parser, rbuffer, strlen(rbuffer), tokens, MAX_JSON_TOKENS);
        for (int i = 0; i < r; i++)
        {
            strcpy(prev_t, t);
            strncpy(t, (char *)((long)rbuffer + tokens[i].start), tokens[i].end - tokens[i].start);
            t[tokens[i].end - tokens[i].start] = 0;
            if (strcmp(prev_t, "broker") == 0)
                strcpy(broker, t);
            if (strcmp(prev_t, "broker_host") == 0)
                strcpy(broker_host, t);
            if (strcmp(prev_t, "broker_port") == 0)
                strcpy(broker_port, t);
            if (strcmp(prev_t, "broker_user") == 0)
                strcpy(broker_user, t);
            if (strcmp(prev_t, "broker_password") == 0)
                strcpy(broker_password, t);
            if (strcmp(prev_t, "broker_queue") == 0)
                strcpy(broker_queue, t);
            if (strcmp(prev_t, "broker_output_queue") == 0)
                strcpy(broker_output_queue, t);
            if (strcmp(prev_t, "database") == 0)
                strcpy(database_path, t);
        }
        free(rbuffer);
        if (database_path[0] == 0)
        {
            printf("Incomplete configuration\n");
            exit(0);
        }
    }

    // Converts date to number for comparison only
    static unsigned long date_to_number(char *ldate) {
        unsigned long result = 0;
        if (strlen(ldate) < 10)
            return 0;
        if (ldate[4] != '-' || ldate[7] != '-')
            return 0;
        char *date = (char*)malloc(128);
        if (!date) out_of_memory();
        strcpy(date, ldate);
        date[4] = 0; date[7] = 0;
        bool has_time = (date[10] == ' ' || date[10] == 'T') ? true : false;
        date[10] = 0;
        result += atol(date) * 12 * 31;
        result += atol((char*)((unsigned long)date + 5)) * 31;
        result += atol((char*)((unsigned long)date + 8));
        result *= 86400;
        if (!has_time) {
            free(date);
            return result;
        }
        if (strlen(ldate) < 19) {
            free(date);
            return result;
        }
        if (date[13] != ':' || date[16] != ':') {
            free(date);
            return result;
        }
        date[13] = 0; date[16] = 0; date[19] = 0;
        result += atol((char*)((unsigned long)date + 11)) * 3600;
        result += atol((char*)((unsigned long)date + 14)) * 60;
        result += atol((char*)((unsigned long)date + 17));
        free(date);
        return result;
    }

    // Instantiate Request class, process request and free memory
    static char *process_request(char *buffer, int operation, int endpoint, int modifier, int *pip);

};

const unsigned char utils::base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
