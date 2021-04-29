
    char *request::save_order(char *subject, char *object, int *status)
    {
        char *message;
        if (this->_n_orders >= N_MAX_ORDERS)
        {
            message = (char *)malloc(128);
            if(!message) utils::out_of_memory();
            sprintf(message, "Too many order sequences");
            *status = -1;
            return message;
        }
        strcpy(this->_orders[this->_n_orders].variable, subject);
        utils::strtolower(object);
        strcpy(this->_orders[this->_n_orders].direction, object);
        this->_n_orders++;
        return NULL;
    };

    char *request::save_filter(char *subject, char *predicate, char *object, int *status, int current_level)
    {
        char *message;
        if (this->_n_filters >= N_MAX_FILTERS)
        {
            message = (char *)malloc(128);
            if(!message) utils::out_of_memory();
            sprintf(message, "Too many filter sequences");
            *status = -1;
            return message;
        }
//printf("filter %i %s - %s - %s\n", current_level, subject, predicate, object);
        // Filters grouping
        if(current_level > 0) {
            if(this->current_filter_group[current_level - 1] == -1) {
                message = (char *)malloc(128);
                if(!message) utils::out_of_memory();
                sprintf(message, "Logic operation (and, or) is not set in the filter group");
                *status = -1;
                return message;
            }
        }
        if(subject[0] && predicate == NULL && object == NULL) {
            utils::strtolower(subject);
            this->_filter_groups[this->_n_filter_groups].logic = ((strcmp(subject, "or") == 0) ? LOGIC_OR : LOGIC_AND);
            this->_filter_groups[this->_n_filter_groups].group = (current_level > 0 ? this->current_filter_group[current_level - 1] : -1);
            this->current_filter_group[current_level] = this->_n_filter_groups;
            this->_n_filter_groups++;
            return NULL;
        }
        strcpy(this->_filters[this->_n_filters].variable, subject);
        utils::strtolower(predicate);
        if(strcmp(predicate, "contains") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_CONTAINS;
        else if(strcmp(predicate, "notcontains") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_NOTCONTAINS;
        else if(strcmp(predicate, "equal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_EQUAL;
        else if(strcmp(predicate, "notequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_NOTEQUAL;
        else if(strcmp(predicate, "icontains") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_ICONTAINS;
        else if(strcmp(predicate, "inotcontains") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_INOTCONTAINS;
        else if(strcmp(predicate, "iequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_IEQUAL;
        else if(strcmp(predicate, "inotequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_INOTEQUAL;
        else if(strcmp(predicate, "more") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_MORE;
        else if(strcmp(predicate, "less") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_LESS;
        else if(strcmp(predicate, "moreorequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_MOREOREQUAL;
        else if(strcmp(predicate, "lessorequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_LESSOREQUAL;
        else if(strcmp(predicate, "exists") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_EXISTS;
        else if(strcmp(predicate, "notexists") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_NOTEXISTS;
        else {
            message = (char *)malloc(128);
            if(!message) utils::out_of_memory();
            sprintf(message, "Unknown filter operation: %s", predicate);
            *status = -1;
            return message;
        }
        if(object)
            strcpy(this->_filters[this->_n_filters].value, object);
        this->_filters[this->_n_filters].group = (current_level > 0 ? this->current_filter_group[current_level - 1] : -1);
        this->_n_filters++;
        return NULL;
    };

    // Find index in the variables array for the given variable
    int request::find_var(char *obj)
    {
        for (int j = 0; j < this->n_var; j++)
        {
            if (strcmp(obj, this->var[j].obj) == 0)
                return j;
        }
        return -1;
    };

    // Add new variable into the array of variables
    int request::add_var(char *obj)
    {
        if (this->n_var >= N_MAX_VARIABLES)
            return -1;
        strcpy(this->var[this->n_var].obj, obj);
        return this->n_var++;
    };

    // Recursively propagates variables dependence
    void request::propagate_dependent(int i, int dep)
    {
        this->depth++;
        if (this->depth > 10)
            return;
        this->var[i].dep += dep;
        for (int j = 0; j < this->var[i].n; j++)
        {
            if (this->var[ this->var[i].cond_o[j] ].obj[0] == '?') {
                bool found = false;
                for (int k=0; k<this->var[ this->var[i].cond_o[j] ].n_dep_of; k++) {
                    if (this->var[ this->var[i].cond_o[j] ].dependent_of[ k ] == i)
                        found = true;
                }
                if (!found)
                    this->var[ this->var[i].cond_o[j] ].dependent_of[ this->var[ this->var[i].cond_o[j] ].n_dep_of++ ] = i;
                propagate_dependent(this->var[i].cond_o[j], this->var[i].dep + 1);
            }
        }
    };

    // Check if possible candidate s has a triple with the given predicate and object
    bool request::check_candidate_match(char *s, char *predicate, int cond_o) {
        bool match = false;
        unsigned long n = 0, *cand;
        if (this->var[ cond_o ].obj[0] == '?') {
            if (!this->var[ cond_o ].n_cand) return true; // Doubtful
            for (int k = 0; k < this->var[ cond_o ].n_cand; k++) {
//printf("\tCheck (1) %s - %s - %s\n", s, predicate, this->var[ cond_o ].cand[k]);
                cand = triples_index::find_matching_triples(s, predicate, this->var[ cond_o ].cand[k], NULL, NULL, &n);
                if (cand != NULL) {
                    free(cand);
                    match = true;
                    break;
                }
            }
        }
        else {
            cand = triples_index::find_matching_triples(s, predicate, this->var[ cond_o ].obj, NULL, NULL, &n);
//printf("\tCheck (2) %s - %s - %s, %i, %x\n", s, predicate, this->var[ cond_o ].obj, n, cand);
            if (cand != NULL) {
                free(cand);
                match = true;
            }
        }
        return match;
    };

    // Add a new candidate object (value) for some variable
    // j is the variable index in this->var, s is the variable value, datatype and lang are the value's metadata
    int request::add_candidate(int j, int i_cond, char *s, char *datatype, char *lang)
    {
        bool found = false;
        for (int k = 0; k < this->var[j].n_cand; k++)
        {
            if (strcmp(this->var[j].cand[k], s) == 0)
                return k;
        }
//printf("add candidate (%i) %s (datatype = %s, lang = %s)\n", j, s, datatype, lang);

        // Check that candidate match other conditions
        bool match = true;
//printf("\nThere are %i conditions for variable %i\n", this->var[j].n, j);
        for (int i = 0; i < this->var[j].n; i++) {
            if (i == i_cond) continue;
            // For an ordinary predicate
//printf("cond_p = %i, j = %i\n", this->var[j].cond_p[i], j);
            if (this->var[j].cond_p[i] >= 0) {
                // If this condition is in another UNION clause, skip it
                if (i_cond != -1) {
                    if (this->var[j].cond_union[i] != -1 && this->var[j].cond_union[i] != this->var[j].cond_union[i_cond] ) {
//printf("union, continue: %i != %i (i = %i, i_cond = %i)\n", this->var[j].cond_union[i], this->var[j].cond_union[i_cond], i, i_cond);
                        continue;
                    }
                }
                match = check_candidate_match(s, this->var[ this->var[j].cond_p[i] ].obj, this->var[j].cond_o[i]);
                if(!match) break;
            }
            // If predicate is a variable
            else if (this->var[j].cond_p[0] == -1) {
                for (int l = 0; l < this->var[ this->var[j].cond_o[i] ].n; l++) {
//printf("inner check %i\n", this->var[ this->var[j].cond_o[i] ].cond_p[l]);
                    if (this->var[ this->var[j].cond_o[i] ].cond_p[l] == -2) {
                        match = check_candidate_match(s, this->var[ this->var[j].cond_o[i] ].obj, this->var[ this->var[j].cond_o[i] ].cond_o[l]);
                        if(!match) break;
                    }
                }
            }
        }
        if(!match) {
//printf("not match\n");
            return -1;
        }

        if (!this->var[j].cand)
        {
            this->var[j].cand = (char **)malloc(1024 * sizeof(char *));
            this->var[j].cand_d = (char **)malloc(1024 * sizeof(char *));
            this->var[j].cand_l = (char **)malloc(1024 * sizeof(char *));
            if(!this->var[j].cand || !this->var[j].cand_d || !this->var[j].cand_l) utils::out_of_memory();
        }
        else if (this->var[j].n_cand % 1024 == 0)
        {
            this->var[j].cand = (char **)realloc(this->var[j].cand, (this->var[j].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[j].cand_d = (char **)realloc(this->var[j].cand, (this->var[j].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[j].cand_l = (char **)realloc(this->var[j].cand, (this->var[j].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            if(!this->var[j].cand || !this->var[j].cand_d || !this->var[j].cand_l) utils::out_of_memory();
        }
        // Add candidate for the j-th variable
        this->var[j].cand[this->var[j].n_cand] = s;
        this->var[j].cand_d[this->var[j].n_cand] = datatype;
        this->var[j].cand_l[this->var[j].n_cand] = lang;
//printf("added %i to %i\n", this->var[j].n_cand, j);
        this->var[j].n_cand++;
        return this->var[j].n_cand - 1;
    }

    // Add solution from the i-th variable to the j-th. Cand (source) is the i-th variable candidate, sol (target) is j-th variable candidate
    void request::add_solution(int i, int j, int cand, int sol, int bearer) {
//printf("\tadd solution: from var %i cand %i = %s to var %i cand %i = %s, bearer = %i\n", i, cand, this->var[i].cand[cand], j, sol, this->var[j].cand[sol], bearer);
        if (!this->var[i].solution[j]) {
            this->var[i].solution[j] = (int*)malloc(1024 * sizeof(int));
            this->var[i].solution_cand[j] = (int*)malloc(1024 * sizeof(int));
            if (!this->var[i].solution[j] || !this->var[i].solution_cand[j]) utils::out_of_memory();
            if (bearer != -1) {
                this->var[i].bearer[j] = (int*)malloc(1024 * sizeof(int));
                if(!this->var[i].bearer[j]) utils::out_of_memory();
            }
        }
        else if (this->var[i].n_sol[j] % 1024 == 0) {
            this->var[i].solution[j] = (int *)realloc(this->var[i].solution[j], (this->var[i].n_sol[j] / 1024 + 1) * 1024 * sizeof(int));
            this->var[i].solution_cand[j] = (int *)realloc(this->var[i].solution_cand[j], (this->var[i].n_sol[j] / 1024 + 1) * 1024 * sizeof(int));
            if(!this->var[i].solution[j] || !this->var[i].solution_cand[j]) utils::out_of_memory();
            if(bearer != -1) {
                this->var[i].bearer[j] = (int *)realloc(this->var[i].bearer[j], (this->var[i].n_sol[j] / 1024 + 1) * 1024 * sizeof(int));
                if(!this->var[i].bearer[j]) utils::out_of_memory();
            }
        }
        bool found = false;
        // Check if solution is unique
        for (int k=0; k<this->var[i].n_sol[j]; k++) {
            if (this->var[i].solution_cand[j][k] == cand && this->var[i].solution[j][k] == sol) {
                if (bearer != -1 && this->var[i].bearer) {
                    if (this->var[i].bearer[j][k] == bearer)
                        found = true;
                }
                else
                    found = true;
            }
        }
        if(found) return;
        this->var[i].solution_cand[j][ this->var[i].n_sol[j] ] = cand;
        this->var[i].solution[j][ this->var[i].n_sol[j] ] = sol;
        if(bearer != -1)
            this->var[i].bearer[j][ this->var[i].n_sol[j] ] = bearer;
        this->var[i].n_sol[j]++;
//printf("\tadded solution_%i to variable %i, bearer = %i, n_sol = %i, sol = %i, cand = %i\n", j, i, bearer, this->var[i].n_sol[j], sol, cand);
    }

    // Fill candidate objects (values) array for j-th variable using its linkage from i-th variable
    bool request::get_candidates(int i, int i_cond, int j, int cand_subject, char *subject, char *predicate, char *object, bool negative, int bearer)
    {
        unsigned long n = 0, *res;
//printf("\nFind %s - %s - %s for i = %i, j = %i, cand = %i, bearer = %i\n", subject, predicate, object, i, j, cand_subject, bearer);
        if (strcmp(predicate, "relation") == 0)
            res = triples_index::find_matching_triples(subject, "*", object, NULL, NULL, &n);
        else if (strcmp(predicate, "relation_value") == 0)
            return false;
        else
            res = triples_index::find_matching_triples(subject, predicate, object, NULL, NULL, &n);
//printf("found %i items\n", n);
        if (negative) {
            free(res);
            if( n > 0 )
                return false;
            return true;
        }
        for (unsigned long k = 0; k < n; k++)
        {
            if (subject[0] == '*')
                add_candidate(i, i_cond, utils::get_string(triples[res[k]].s_pos), NULL, NULL);
            else if (predicate[0] == '*') {
                int ind_cand = add_candidate(j, i_cond, utils::get_string(triples[res[k]].p_pos), NULL, NULL);
//printf("predicate = %s, ind_cand = %i\n", utils::get_string(triples[res[k]].p_pos), ind_cand);
                if (i >= 0 && ind_cand > -1)
                    add_solution(i, j, cand_subject, ind_cand, bearer);
            }
            else if (object[0] == '*') {
                int ind_cand = add_candidate(j, i_cond, utils::get_string(triples[res[k]].o_pos), triples[res[k]].d_pos ? utils::get_string(triples[res[k]].d_pos) : NULL, triples[res[k]].l);
//printf("add candidate %s to variable %s, ind_cand = %i\n", utils::get_string(triples[res[k]].o_pos), this->var[j].obj, ind_cand);
                if (i >= 0 && ind_cand > -1)
                    add_solution(i, j, cand_subject, ind_cand, bearer);
            }
        }
        free(res);
        if( n > 0 ) return true;
        return false;
    }

    // Add new combination to the i-th variable combinations
    void request::push_combination(int i, int *comb) {
        if (this->var[i].n_comb == 0) {
            this->var[i].comb_value = (int **)malloc(1024 * sizeof(int*));
            if(!this->var[i].comb_value) utils::out_of_memory();
        }
        else if (this->var[i].n_comb % 1024 == 0) {
            this->var[i].comb_value = (int **)realloc(this->var[i].comb_value, (this->var[i].n_comb / 1024 + 1) * 1024 * sizeof(int *));
            if(!this->var[i].comb_value) utils::out_of_memory();
        }
        // Check if this combination already exists
        for (int j = 0; j < this->var[i].n_comb; j++) {
            bool identical = true;
            for (int k = 0; k < N_MAX_VARIABLES; k++) {
                if (comb[k] != this->var[i].comb_value[j][k]) {
                    identical = false;
                    break;
                }
            }
            if (identical)
                return;
        }
        this->var[i].comb_value[this->var[i].n_comb] = (int *)malloc(N_MAX_VARIABLES * sizeof(int));
        if (!this->var[i].comb_value[this->var[i].n_comb]) utils::out_of_memory();
        memcpy(this->var[i].comb_value[this->var[i].n_comb], comb, sizeof(int)*N_MAX_VARIABLES);
        this->var[i].n_comb++;
    }

    // Recursively build combinations for the i-th variable, id candidate
    void request::build_combination(int i, int id, int *comb, int ind_cond, int bearer) {
        if (!this->var[i].cand) return;
        if (!this->var[i].cand[id]) return;
//printf("BC Build combinations for %s, candidate %i, cond %i\n", this->var[i].obj, id, ind_cond);
        if(this->var[ this->var[i].cond_o[ind_cond] ].obj[0] != '?') {
            if(ind_cond > this->var[i].n - 1)
                this->push_combination(i, comb);
            else
                this->build_combination(i, id, comb, ind_cond+1, bearer);
        }
        bool relation = false;
        int dependent_cond = 0;
        for (int y = 0; y < this->var[i].n; y++) {
            if (this->var[i].cond_p[y] == -2) {
                relation = true;
                for (int z = 0; z < this->var[ this->var[i].dependent_of[0] ].n; z++) {
                    if (this->var[ this->var[i].dependent_of[0] ].cond_p[z] == -1 && this->var[ this->var[i].dependent_of[0] ].cond_o[z] == i)
                        dependent_cond = z;
                }
            }
        }
        int value_var = this->var[i].cond_o[ind_cond];
//printf("BC value_var = %s, %i solutions\n", this->var[value_var].obj, this->var[i].n_sol[value_var]);
        // For the predicate variables
        if (relation) {
            if (this->var[i].n_sol[value_var] > 0) {
//printf("BC value_var = %i, dependent of = %i, dependent_cond = %i\n", value_var, this->var[i].dependent_of[0], dependent_cond);
                for (int j = 0; j < this->var[i].n_sol[value_var]; j++) {
//printf("BC Build for solution (rel) %s = %s, id = %i of variable %i, bearer is %i\n", this->var[value_var].obj, this->var[value_var].cand[ this->var[i].solution[value_var][j] ], id, i, bearer);
                    if (this->var[i].solution[value_var][j] >= 0 && this->var[value_var].n_cand > this->var[i].solution[value_var][j]) {
                        if (bearer != -1 && this->var[i].bearer) {
                            if (this->var[i].bearer[ value_var ][j] == bearer) {
//printf("BC Bearer this->var[%i].bearer[ %i ][%i] = %i, bearer match: %s = %s, %s = %s\n", i, value_var, j, this->var[i].bearer[ value_var ][j], this->var[i].obj, this->var[i].cand[this->var[i].solution_cand[value_var][j]], this->var[value_var].obj, this->var[value_var].cand[this->var[i].solution[value_var][j]]);
                                comb[i] = this->var[i].solution_cand[value_var][j];
                                comb[value_var] = this->var[i].solution[value_var][j];
                            }
                        }
                    }
                    if(ind_cond > this->var[i].n - 1)
                        this->push_combination(i, comb);
                    else
                        this->build_combination(i, id, comb, ind_cond+1, bearer);
                }
            }
            else {
                if(ind_cond > this->var[i].n - 1)
                    this->push_combination(i, comb);
                else
                    this->build_combination(i, id, comb, ind_cond+1, bearer);
            }
        }
        // For the ordinary variables
        else {
            if (this->var[i].n_sol[value_var] > 0) {
                bool found = false;
                // Cycle through the solutions to the target variable from the current candidate object
                for (int j = 0; j < this->var[i].n_sol[value_var]; j++) {
                    if (this->var[i].solution_cand[value_var][j] != id) continue;
                    if (this->var[value_var].cand[this->var[i].solution[value_var][j]] == 0)
                        continue;
                    comb[value_var] = this->var[i].solution[value_var][j];
                    found = true;
                    if(ind_cond > this->var[i].n - 1)
                        this->push_combination(i, comb);
                    else
                        this->build_combination(i, id, comb, ind_cond+1, bearer);
                }
                // If there is no solutions to the target variable from the current candidate, but the variable is optional - go on further
                if (!found && (this->var[value_var].optional || this->var[i].cond_union[ind_cond] != -1)) {
                    comb[value_var] = -1;
                    if(ind_cond > this->var[i].n - 1)
                        this->push_combination(i, comb);
                    else
                        this->build_combination(i, id, comb, ind_cond+1, bearer);
                }
            }
            else {
                if(ind_cond > this->var[i].n - 1)
                    this->push_combination(i, comb);
                else
                    this->build_combination(i, id, comb, ind_cond+1, bearer);
            }
        }
    }

    void request::global_push_combination(int i, int j) {
        if (this->n_pcv == 0) {
            this->pre_comb_value = (char ***)malloc(1024 * sizeof(char **));
            this->pre_comb_value_type = (char ***)malloc(1024 * sizeof(char **));
            this->pre_comb_value_lang = (char ***)malloc(1024 * sizeof(char **));
            if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) utils::out_of_memory();
        }
        else if (this->n_pcv % 1024 == 0) {
            this->pre_comb_value = (char ***)realloc(this->pre_comb_value, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
            this->pre_comb_value_type = (char ***)realloc(this->pre_comb_value_type, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
            this->pre_comb_value_lang = (char ***)realloc(this->pre_comb_value_lang, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
            if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) utils::out_of_memory();
        }
        this->pre_comb_value[this->n_pcv] = (char **)malloc(N_MAX_VARIABLES * sizeof(char *));
        if(!this->pre_comb_value[this->n_pcv]) utils::out_of_memory();
        memset(this->pre_comb_value[this->n_pcv], 0, N_MAX_VARIABLES * sizeof(char *));
        this->pre_comb_value_type[this->n_pcv] = (char **)malloc(N_MAX_VARIABLES * sizeof(char *));
        if(!this->pre_comb_value_type[this->n_pcv]) utils::out_of_memory();
        memset(this->pre_comb_value_type[this->n_pcv], 0, N_MAX_VARIABLES * sizeof(char *));
        this->pre_comb_value_lang[this->n_pcv] = (char **)malloc(N_MAX_VARIABLES * sizeof(char *));
        if(!this->pre_comb_value_lang[this->n_pcv]) utils::out_of_memory();
        memset(this->pre_comb_value_lang[this->n_pcv], 0, N_MAX_VARIABLES * sizeof(char *));
        for (int l = 0; l < this->n_var; l++) {
            if (this->var[i].comb_value[j][l] < 0) continue;
            this->pre_comb_value[this->n_pcv][l] = this->var[l].cand[ this->var[i].comb_value[j][l] ];
            if (this->var[l].cand_d[ this->var[i].comb_value[j][l] ]) {
                this->pre_comb_value_type[this->n_pcv][l] = this->var[l].cand_d[ this->var[i].comb_value[j][l] ];
            }
            if (this->var[l].cand_l[ this->var[i].comb_value[j][l] ]) {
                if(this->var[l].cand_l[ this->var[i].comb_value[j][l] ][0]) {
                    this->pre_comb_value_lang[this->n_pcv][l] = this->var[l].cand_l[ this->var[i].comb_value[j][l] ];
                }
            }
        }
        this->n_pcv++;
    }

    // Entry point for triples chain request
    char *request::return_triples_chain(char *reqid)
    {
        int order[N_MAX_ORDERS], max_dep = 0, n = 0;
        char star[2] = "*", relation[9] = "relation", relation_value[15] = "relation_value";
        memset(order, 0, sizeof(int) * N_MAX_ORDERS);
        // Fill the variables array
//printf("Patterns: %i\n", this->_n_triples);
        for (int i = 0; i < this->_n_triples; i++)
        {
            int ics = find_var(this->_triples[i].s);
            if (ics == -1)
                ics = add_var(this->_triples[i].s);
            int icp = find_var(this->_triples[i].p);
            if (icp == -1)
                icp = add_var(this->_triples[i].p);
            int ico = find_var(this->_triples[i].o);
            if (ico == -1)
                ico = add_var(this->_triples[i].o);
            if (ics == -1 || icp == -1 || ico == -1)
                continue;
            // If predicate is a variable, add condition for predicate
            if (this->_triples[i].p[0] == '?') {
                this->var[ics].cond_union[this->var[ics].n] = this->_triples[i]._union;
                this->var[ics].cond_p[this->var[ics].n] = -1;
                this->var[ics].cond_o[this->var[ics].n++] = icp;
                this->var[icp].dep++;
                this->var[icp].cond_union[this->var[icp].n] = this->_triples[i]._union;
                this->var[icp].cond_p[this->var[icp].n] = -2;
                this->var[icp].cond_o[this->var[icp].n++] = ico;
                this->var[ico].dep++;
            }
            // Otherwise add a "normal" condition
            else {
                this->var[ics].cond_union[this->var[ics].n] = this->_triples[i]._union;
                this->var[ics].cond_p[this->var[ics].n] = icp;
                this->var[ics].cond_o[this->var[ics].n++] = ico;
                if (this->var[icp].obj[0] == '?')
                    this->var[icp].dep++;
                if (this->var[ico].obj[0] == '?')
                    this->var[ico].dep++;
            }
        }
        // Mark optional variables
        for (int i = 0; i < this->n_optional; i++) {
            for (int j = 0; j < this->n_var; j++) {
                if (strcmp(this->optional[i], this->var[j].obj) == 0) {
                    this->var[j].optional = true;
                    this->var[j].optional_group = this->optional_group[i];
                }
            }
        }
        // Find variables dependence to define the variables resolution order
        for (int i = 0; i < this->n_var; i++)
        {
            for (int j = 0; j < this->var[i].n; j++)
            {
                if (this->var[i].dep == 0 && this->var[ this->var[i].cond_o[j] ].obj[0] == '?') {
                    bool found = false;
                    for (int k=0; k<this->var[ this->var[i].cond_o[j] ].n_dep_of; k++) {
                        if (this->var[ this->var[i].cond_o[j] ].dependent_of[ k ] == i)
                            found = true;
                    }
                    if (!found)
                        this->var[ this->var[i].cond_o[j] ].dependent_of[ this->var[ this->var[i].cond_o[j] ].n_dep_of++ ] = i;
                    propagate_dependent(this->var[i].cond_o[j], 1);
                    if (this->var[ this->var[i].cond_p[j] ].obj[0] == '?') {
                        found = false;
                        for (int k=0; k<this->var[ this->var[i].cond_p[j] ].n_dep_of; k++) {
                            if (this->var[ this->var[i].cond_p[j] ].dependent_of[ k ] == i)
                                found = true;
                        }
                        if (!found)
                        this->var[ this->var[i].cond_p[j] ].dependent_of[ this->var[ this->var[i].cond_p[j] ].n_dep_of++ ] = i;
                        propagate_dependent(this->var[i].cond_p[j], 1);
                    }
                }
            }
        }
        // Search for maximal dependence
        for (int i = 0; i < this->n_var; i++)
        {
            if (this->var[i].dep > max_dep)
                max_dep = this->var[i].dep;
                // Along with that, mark variables that must not exist
                for(int j = 0; j < this->_n_filters; j++) {
                    if(this->_filters[j].operation == COMPARE_NOTEXISTS && strcmp(this->_filters[j].variable, this->var[i].obj) == 0)
                        this->var[i].notexists = true;
                }
        }
        // Build an array with the variables processing order
        for (int i = 0; i <= max_dep; i++)
        {
            for (int j = 0; j < this->n_var; j++)
            {
                if (this->var[j].dep == i)
                    order[n++] = j;
            }
        }

        // Consequently find variable values
        for (int x = 0; x < n; x++)
        {
            int i = order[x];
//printf("Find value for variable %i\n", i);
            char *subject = this->var[i].obj;
            // Cycle the conditions on some subject
            for (int j = 0; j < this->var[i].n; j++)
            {
                char *predicate = this->var[i].cond_p[j] == -1 ? relation : ( this->var[i].cond_p[j] == -2 ? relation_value : this->var[this->var[i].cond_p[j]].obj);
                char *object = this->var[this->var[i].cond_o[j]].obj;
//printf("\nCondition from %i to %i, j = %i. Subject = %s, %i candidates\n", i, this->var[i].cond_o[j], j, subject, this->var[i].n_cand);
                if (subject[0] == '?') 
                {
                    bool match = false;
                    // If there is no candadates OR the current condition is in UNION clause, then we probably can find new candidates - fo scan for them.
                    if (this->var[i].n_cand == 0 || this->var[i].cond_union[j] != -1)
                        match = get_candidates(i, j, this->var[i].cond_o[j], 0, subject[0] == '?' ? star : subject, predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object, this->var[i].notexists, -1);
                    int limit_cand = this->var[i].n_cand; // Number of candidates for the current variable. Saved to avoid checking new candidates added in this cycle.
//printf("=== There are %i candidates, predicate = %s\n", limit_cand, predicate);
                    for (int l = 0; l < limit_cand; l++) {
                        if (this->var[i].cand[l] == NULL) continue;
                        if (predicate == relation) {
                            // If predicate is relating to the specific object, use it
                            char *specific_object = NULL;
                            for (int v = 0; v < this->var[this->var[i].cond_o[j]].n; v++ ) {
                                if (this->var[this->var[i].cond_o[j]].cond_p[v] == -2 && this->var[this->var[this->var[i].cond_o[j]].cond_o[v]].obj[0] != '?') {
                                    specific_object = this->var[this->var[this->var[i].cond_o[j]].cond_o[v]].obj;
                                }
                            }
                            match = get_candidates(i, j, this->var[i].cond_o[j], l, this->var[i].cand[l], object[0] == '?' ? star : object, specific_object ? specific_object : star, this->var[this->var[i].cond_o[j]].notexists, -1);
                        }
                        else if (predicate == relation_value) {
//printf("===========\ndependent of %i variables\n", this->var[i].n_dep_of);
                            for (int n = 0; n < this->var[i].n_dep_of; n++) {
                                int dep_of = this->var[i].dependent_of[n];
                                int limit = this->var[dep_of].n_sol[i];
                                bool found = false;
                                for (int m = 0; m < limit; m++) {
                                    if (l != this->var[dep_of].solution[i][m]) continue;
//printf("m = %i: Parent candidate (bearer) is %s\n", m, this->var[dep_of].cand[ this->var[dep_of].solution_cand[i][m] ]);
                                    match = get_candidates(i, -1, this->var[i].cond_o[j], l, this->var[dep_of].cand[ this->var[dep_of].solution_cand[i][m] ], this->var[i].cand[l], object[0] == '?' ? star : object, this->var[this->var[i].cond_o[j]].notexists, this->var[dep_of].solution_cand[i][m]);
                                    if (match)
                                        found = true;
                                }
                                if ((!found && !this->var[ this->var[i].cond_o[j] ].notexists) || (found && this->var[ this->var[i].cond_o[j] ].notexists)) {
                                    this->var[i].cand[l] = NULL;
                                }
                            }
                        }
                        else
                            match = get_candidates(i, j, this->var[i].cond_o[j], l, this->var[i].cand[l], predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object, this->var[this->var[i].cond_o[j]].notexists, -1);
                        // Inverse condition - matching objects shall not exist
                        if( this->var[ this->var[i].cond_o[j] ].notexists && !match ) {
                            this->var[i].cand[l] = NULL;
                        }
                    }
                }
                else if (subject[0] != '?') add_candidate(i, -1, subject, NULL, NULL);
            }
        }
    // Remove references to non-existant variables
    for (int i = 0; i < n; i++) {
        for(int j=0; j<this->var[i].n; j++) {
            bool notexists = false;
            if (this->var[i].cond_p[j] >= 0) {
                if (this->var[this->var[i].cond_p[j]].notexists)
                    notexists = true;
            }
            if (notexists || this->var[this->var[i].cond_o[j]].notexists) {
                if (j < this->var[i].n-1) {
                    memmove(&this->var[i].cond_p[j], &this->var[i].cond_p[j+1], sizeof(int)*(this->var[i].n - j));
                    memmove(&this->var[i].cond_o[j], &this->var[i].cond_o[j+1], sizeof(int)*(this->var[i].n - j));
                }
                this->var[i].n --;
            }
        }
    }
    //Clean up incomplete solutions
    for (int x = 0; x < n; x++) {
        int i = order[x];
        if (this->var[i].obj[0] != '?' && this->var[i].n == 0)
            continue;
        for (int k = 0; k < N_MAX_VARIABLES; k++) {
            for (int j = 0; j < this->var[i].n_sol[k]; j++) {
                if (this->var[i].solution[k][j] < 0) {
                    if (j < this->var[i].n_sol[k] - 1) {
                        memmove(&this->var[i].solution[k][j], &this->var[i].solution[k][j+1], (this->var[i].n_sol[k] - j) * sizeof(int));
                        memmove(&this->var[i].solution_cand[k][j], &this->var[i].solution_cand[k][j+1], (this->var[i].n_sol[k] - j) * sizeof(int));
                        if(this->var[i].bearer[k])
                            memmove(&this->var[i].bearer[k][j], &this->var[i].bearer[k][j+1], (this->var[i].n_sol[k] - j) * sizeof(int));
                    }
                    this->var[i].n_sol[k]--;
                    j--;
                }
            }
        }
    }
#ifdef PRINT_DIAGNOSTICS
printf("Variables:\n");
for(int x=0; x<n; x++) {
    int i = order[x];
    if(this->var[i].obj[0] != '?' && this->var[i].n == 0)
        continue;
    printf("%i:\t%s, %i conditions, dep = %i, optional = %i, optional_group = %i, notexists = %i\n", i, this->var[i].obj, this->var[i].n,this->var[i].dep, this->var[i].optional, this->var[i].optional_group, this->var[i].notexists);
    for(int j=0; j<this->var[i].n; j++)
        printf("\t\t%s => %s (%i)\n", this->var[i].cond_p[j] == -1 ? "relation" : ( this->var[i].cond_p[j] == -2 ? relation_value : this->var[this->var[i].cond_p[j]].obj), this->var[this->var[i].cond_o[j]].obj, this->var[i].cond_union[j]);
    if(this->var[i].n_dep_of) {
        printf("\tDependent of: ");
        for(int j=0; j<this->var[i].n_dep_of; j++)
            printf(" %i", this->var[i].dependent_of[j]);
        printf("\n");
    }
    if(!this->var[i].n_cand) continue;
    printf("\tCandidates:\n");
    for(int j=0; j<this->var[i].n_cand; j++) {
        if(!this->var[i].cand[j]) continue;
        printf("\t\t%s\n", this->var[i].cand[j]);
    }
    printf("\tSolutions:\n");
    for(int k=0; k<N_MAX_VARIABLES; k++) {
        if(this->var[i].n_sol[k] == 0) continue;
        printf("\t\t%s: %i\n", this->var[k].obj, this->var[i].n_sol[k]);
        for(int j=0; j<this->var[i].n_sol[k]; j++) {
            if(this->var[i].solution[k][j] >= 0 && this->var[k].n_cand > this->var[i].solution[k][j]) {
                if(this->var[k].cand[ this->var[i].solution[k][j] ]) {
                    printf("\t\t\t%s\t", this->var[i].cand[ this->var[i].solution_cand[k][j] ]);
                    printf("%s", this->var[k].cand[ this->var[i].solution[k][j] ] );
                }
                else continue;
            }
            else continue;
            if(this->var[i].bearer[k]) {
                if(this->var[i].bearer[k][j] != -1)
                    printf(" (%s)", this->var[ this->var[i].dependent_of[0] ].cand[ this->var[i].bearer[k][j] ]);
            }
            printf("\n");
        }
    }
}

printf("Filter groups: %i\n", this->_n_filter_groups);
for(int i=0; i<this->_n_filter_groups; i++) {
    printf("%i: %i, %i\n", i, this->_filter_groups[i].group, this->_filter_groups[i].logic);
}

printf("Filters: %i\n", this->_n_filters);
for(int i=0; i<this->_n_filters; i++) {
    printf("%i (%i): %s - %i - %s\n", i, this->_filters[i].group, this->_filters[i].variable, this->_filters[i].operation, this->_filters[i].value);
}
#endif
        // Build combinations for each variable having candidates and dependent variables
        for (int x = 0; x < n; x++)
        {
            int i = order[x];
            if (this->var[i].obj[0] != '?' || this->var[i].n_cand == 0 || this->var[i].notexists)
                continue;
//printf("========= %i\n", i);
            // Check if there is at least one dependent variable
            if (max_dep > 0) {
                bool found = false;
                for (int y = 0; y < n; y++) {
                    for (int z = 0; z < this->var[y].n_dep_of; z++) {
                        if (this->var[y].dependent_of[z] == i)
                            found = true;
                    }
                }
                if(!found) continue;
            }
            bool is_relation = false;
            for (int y = 0; y < this->var[i].n; y++) {
                if (this->var[i].cond_p[y] == -2)
                    is_relation = true;
            }
            // Cycle through candidates and build combination starting with each of them
            // For predicate variables
            if(is_relation) {
                int dep = this->var[i].dependent_of[0];
//printf("Variable %i, of which variable %i depends, has %i candidates\n", dep, i, this->var[dep].n_cand);
                for (int y = 0; y < this->var[dep].n_cand; y++) {
                    if (this->var[dep].cand[y] == 0) continue;
                    int combination[N_MAX_VARIABLES];
                    memset(&combination, -1, sizeof(int)*N_MAX_VARIABLES);
                    combination[dep] = y; // Put source object (bearer) to the combination
//printf("Source candidate object %s\nThere are %i solutions from %s to %s\n", this->var[dep].cand[y], this->var[dep].n_sol[i], this->var[dep].obj, this->var[i].obj);
                    // Cycle through solutions from the parent variable to the current variable
                    for (int z = 0; z < this->var[dep].n_sol[i]; z++) {
                        if (this->var[dep].solution_cand[i][z] != y) continue;
                        // Find out next variable which represent object in triples where the current variable is a predicate
                        int next_var = -1;
                        // Find the variable to which this predicate refers (a variable representing object)
                        for (int v = 0; v < this->var[i].n; v++) {
//printf("v = %i\n", v);
                            if (this->var[i].cond_p[v] != -2) continue;
                            next_var = this->var[i].cond_o[v];
//printf("next_var = %i\n", next_var);
                            if (next_var == -1) continue;
//printf("next var is %s\n", this->var[next_var].obj);
                            // Check if there is a solution with this source object and predicate
//printf("Check if among the solutions from %s to %s is the source variable's candidate %s as bearer\n", this->var[i].obj, this->var[next_var].obj, this->var[dep].cand[y]);
                            bool found = false;
                            if (this->var[i].bearer) {
                                for (int n = 0; n < this->var[i].n_sol[next_var]; n++) {
                                    if (this->var[i].bearer[next_var][n] == y) {
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found)
                                continue;
                            if (!this->var[dep].solution) continue;
                            if (this->var[dep].solution[i][z] == -1)
                                continue;
//printf("Go build combinations for variable %s and predicate %s, solution %i, while bearer is %s\n", this->var[i].obj, this->var[dep].obj, this->var[dep].solution[i][z], this->var[dep].cand[y]);
                            build_combination(i, this->var[dep].solution[i][z], combination, 0, y);
                        }
                    }
                }
            }
            // For ordinary variables
            else {
                for (int y = 0; y < this->var[i].n_cand; y++) {
                    if (this->var[i].cand[y] == NULL) continue;
                    int combination[N_MAX_VARIABLES];
                    memset(&combination, -1, sizeof(int)*N_MAX_VARIABLES);
                    combination[i] = y;
                    build_combination(i, y, combination, 0, 1);
                }
            }
        }
        // Join combinations of dependent variables from the end of dependency tree to the start
        for (int x = n-1; x >= 0; x--)
        {
            int i = order[x];
            for (int y = 0; y < n; y++) {
                bool is_dependent = false;
                for (int z = 0; z < this->var[y].n_dep_of; z++) {
                    if (this->var[y].dependent_of[z] == i) {
                        is_dependent = true;
                        break;
                    }
                }
                if (!is_dependent) continue;
                int limit = this->var[i].n_comb;
                // If there is no combinations in the parent variable, just copy them from the child one
                if (limit == 0) {
                    for (int k = 0; k < this->var[y].n_comb; k++) {
                        if (this->var[y].comb_value[k][0] == -2) continue;
                        push_combination(i, this->var[y].comb_value[k]);
                    }
                    continue;
                }
                for (int j = 0; j < limit; j++) {
                    if (this->var[i].comb_value[j][0] == -2) continue;
                    bool unset = false;
                    for (int k = 0; k < this->var[y].n_comb; k++) {
                        bool incompatible = false;
                        int newcomb[N_MAX_VARIABLES];
                        memset(newcomb, -1, N_MAX_VARIABLES*sizeof(int));
                        for (int z = 0; z < n; z++) {
                            if (this->var[i].comb_value[j][z] != -1 && this->var[y].comb_value[k][z] != -1 && this->var[i].comb_value[j][z] != this->var[y].comb_value[k][z]) {
                                incompatible = true;
                                break;
                            }
                            if (this->var[i].comb_value[j][z] != -1)
                                newcomb[z] = this->var[i].comb_value[j][z];
                            else if (var[y].comb_value[k][z] != -1)
                                newcomb[z] = this->var[y].comb_value[k][z];
                        }
                        if (!incompatible) {
                            push_combination(i, newcomb);
                            unset = true;
                        }
                    }
                    if (unset)
                        this->var[i].comb_value[j][0] = -2;
                }
            }
        }
/*
printf("\nCombinations for variables:\n");
for (int x = 0; x < n; x++)
{
    int i = order[x];
    if(this->var[i].n_comb == 0) continue;
    printf("\nVariable: %s, %i combinations\n", this->var[i].obj, this->var[i].n_comb);
    for(int y = 0; y < this->var[i].n_comb; y++) {
        if(this->var[i].comb_value[y][0] == -2) continue;
        printf("\t");
        for(int z = 0; z < N_MAX_VARIABLES; z++) {
            if(this->var[i].comb_value[y][z] != -1)
                printf("%s = %s, ", this->var[z].obj, this->var[z].cand[this->var[i].comb_value[y][z]]);
        }
        printf("\n");
    }
}
*/
        // Push the combination of the independent variables into the global array of combinations, and transform candidates indexes into URIs or literal values
        for (int i = 0; i < n; i++) {
            if (this->var[i].dep > 0) continue;
            for (int j = 0; j < this->var[i].n_comb; j++) {
                if (this->var[i].comb_value[j][0] != -2)
                    global_push_combination(i, j);
            }
        }

        for (int i = 0; i < n; i++) {
            if (this->var[i].cand) free(this->var[i].cand);
            if (this->var[i].cand_s) free(this->var[i].cand_s);
            if (this->var[i].cand_p) free(this->var[i].cand_p);
            if (this->var[i].cand_d) free(this->var[i].cand_d);
            if (this->var[i].cand_l) free(this->var[i].cand_l);
            for (int j = 0; j < n; j++) {
                if (this->var[i].solution[j]) free(this->var[i].solution[j]);
                if (this->var[i].solution_cand[j]) free(this->var[i].solution_cand[j]);
                if (this->var[i].bearer[j]) free(this->var[i].bearer[j]);
            }
            if (this->var[i].comb_value) {
                for (int j = 0; j < this->var[i].n_comb; i++)
                    free(this->var[i].comb_value[j]);
                free(this->var[i].comb_value);
            }
        }

        // Cycle through the combinations generated for independent variables to make complete ones
        for (int z = 0; z < this->n_pcv; z++)
        {
            // Cycle through all remaining combinations
            int npcv = this->n_pcv;
            for (int x = z + 1; x < npcv; x++)
            {
                bool compat = true, inequal = false;
                // Every non-empty element of the compatible remaining combination must be present in the source combination, if the value of the same variable is set in the source combination
                for (int v = 0; v < N_MAX_VARIABLES; v++)
                {
                    if (this->pre_comb_value[z][v] == NULL)
                    {
                        if (this->pre_comb_value[x][v] != NULL)
                            inequal = true;
                        continue;
                    }
                    if (this->pre_comb_value[x][v] == NULL)
                        continue;
                    if (strcmp(this->pre_comb_value[z][v], this->pre_comb_value[x][v]) != 0)
                    {
                        compat = false;
                        break;
                    }
                }
                // Sub-combinations are compatible and not equal, let us join them
                if (compat && inequal)
                {
                    if (this->n_pcv == 0) {
                        this->pre_comb_value = (char ***)malloc(1024 * sizeof(char **));
                        this->pre_comb_value_type = (char ***)malloc(1024 * sizeof(char **));
                        this->pre_comb_value_lang = (char ***)malloc(1024 * sizeof(char **));
                        if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) utils::out_of_memory();
                    }
                    else if (this->n_pcv % 1024 == 0) {
                        this->pre_comb_value = (char ***)realloc(this->pre_comb_value, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                        this->pre_comb_value_type = (char ***)realloc(this->pre_comb_value_type, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                        this->pre_comb_value_lang = (char ***)realloc(this->pre_comb_value_lang, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                        if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) utils::out_of_memory();
                    }
                    this->pre_comb_value[this->n_pcv] = (char **)malloc(N_MAX_VARIABLES * sizeof(char *));
                    if(!this->pre_comb_value[this->n_pcv]) utils::out_of_memory();
                    memset(this->pre_comb_value[this->n_pcv], 0, N_MAX_VARIABLES * sizeof(char *));
                    this->pre_comb_value_type[this->n_pcv] = (char **)malloc(N_MAX_VARIABLES * sizeof(char *));
                    if(!this->pre_comb_value_type[this->n_pcv]) utils::out_of_memory();
                    memset(this->pre_comb_value_type[this->n_pcv], 0, N_MAX_VARIABLES * sizeof(char *));
                    this->pre_comb_value_lang[this->n_pcv] = (char **)malloc(N_MAX_VARIABLES * sizeof(char *));
                    if(!this->pre_comb_value_lang[this->n_pcv]) utils::out_of_memory();
                    memset(this->pre_comb_value_lang[this->n_pcv], 0, N_MAX_VARIABLES * sizeof(char *));
                    for (int v = 0; v < N_MAX_VARIABLES; v++)
                    {
                        if (this->pre_comb_value[z][v] != NULL) {
                            this->pre_comb_value[this->n_pcv][v] = this->pre_comb_value[z][v];
                            this->pre_comb_value_type[this->n_pcv][v] = this->pre_comb_value_type[z][v];
                            this->pre_comb_value_lang[this->n_pcv][v] = this->pre_comb_value_lang[z][v];
                        }
                        if (this->pre_comb_value[x][v] != NULL) {
                            this->pre_comb_value[this->n_pcv][v] = this->pre_comb_value[x][v];
                            this->pre_comb_value_type[this->n_pcv][v] = this->pre_comb_value_type[x][v];
                            this->pre_comb_value_lang[this->n_pcv][v] = this->pre_comb_value_lang[x][v];
                        }
                    }
                    this->n_pcv++;
                }
            }
        }

#ifdef PRINT_DIAGNOSTICS
printf("\nCombinations:\n");
for (int x = 0; x < this->n_pcv; x++) {
    printf("%i. ", x);
    if (this->pre_comb_value[x][0] == NULL)
        printf("(deleted)");
    for (int l = 0; l < this->n_var; l++) {
        if (this->pre_comb_value[x][l])
            printf("%s%s = %s", (l!=0?", ":""), this->var[l].obj, this->pre_comb_value[x][l]);
    }
    printf("\n\n");
}
#endif

        // Apply filters
        for (int z = 0; z < this->n_pcv; z++) {
//printf("\nCombination %i\n", z);
            bool filter_match[N_MAX_FILTERS], incomplete = false, fixed_lvalue = false, fixed_rvalue = false;
            int group_match[N_MAX_FILTER_GROUPS], res;
            memset(filter_match, 0, N_MAX_FILTERS*sizeof(bool)); memset(group_match, 0, N_MAX_FILTER_GROUPS*sizeof(int));
            // First, let us check each of the filters
            bool is_date = false;
            for(int i=0; i<this->_n_filters; i++) {
                char *lvalue = NULL, *rvalue = NULL;
                if(this->_filters[i].variable[0] == '?') {
                    for (int x = 0; x < n; x++) {
                        if (strcmp(this->var[x].obj, this->_filters[i].variable) == 0) {
                            lvalue = this->pre_comb_value[z][x];
                            if (this->pre_comb_value_type[z][x]) {
                                if (strncmp(this->pre_comb_value_type[z][x], "http://www.w3.org/2001/XMLSchema#date", 37) == 0)
                                    is_date = true;
                            }
                            break;
                        }
                    }
                }
                else {
                    lvalue = this->_filters[i].variable;
                    fixed_lvalue = true;
                }
                if(this->_filters[i].value[0] == '?') {
                    for (int x = 0; x < n; x++) {
                        if (strcmp(this->var[x].obj, this->_filters[i].value) == 0) {
                            rvalue = this->pre_comb_value[z][x];
                            if (this->pre_comb_value_type[z][x]) {
                                if (strncmp(this->pre_comb_value_type[z][x], "http://www.w3.org/2001/XMLSchema#date", 37) == 0)
                                    is_date = true;
                            }
                            break;
                        }
                    }
                }
                else {
                    rvalue = this->_filters[i].value;
                    fixed_rvalue = true;
                }
//printf("filter %i, lvalue: %s, rvalue: %s\n", i, lvalue, rvalue);
                if( (!lvalue || !rvalue ) && this->_filters[i].operation != COMPARE_NOTEXISTS) {
                    incomplete = true;
                    break;
                }
                char *lv, *rv;
                unsigned long l_lv, l_rv;
                if (is_date) {
                    l_lv = utils::date_to_number(lvalue);
                    l_rv = utils::date_to_number(rvalue);
                }
                // Cast case for case-insensitive operations
                if(this->_filters[i].operation == COMPARE_IEQUAL || this->_filters[i].operation == COMPARE_INOTEQUAL || this->_filters[i].operation == COMPARE_ICONTAINS || this->_filters[i].operation == COMPARE_NOTCONTAINS) {
                    lv = (char*)malloc(strlen(lvalue)+1);
                    rv = (char*)malloc(strlen(rvalue)+1);
                    if(!lv || !rv) utils::out_of_memory();
                    strcpy(lv, lvalue); strcpy(rv, rvalue);
                    utils::strtolower(lv); utils::strtolower(rv);
                }
                bool result = false;
                switch(this->_filters[i].operation) {
                    case COMPARE_CONTAINS:
                        if(strstr(lvalue, rvalue))
                            result = true;
                        break;
                    case COMPARE_NOTCONTAINS:
                        if(!strstr(lvalue, rvalue))
                            result = true;
                        break;
                    case COMPARE_ICONTAINS:
                        if(strstr(lv, rv))
                            result = true;
                        break;
                    case COMPARE_INOTCONTAINS:
                        if(!strstr(lv, rv))
                            result = true;
                        break;
                    case COMPARE_EXISTS:
                        if(lvalue[0])
                            result = true;
                        break;
                    case COMPARE_NOTEXISTS:
                        result = true;
                        break;
                    case COMPARE_EQUAL:
                    case COMPARE_NOTEQUAL:
                        if (is_date) {
                            if ((l_lv == l_rv) && (this->_filters[i].operation == COMPARE_EQUAL))
                                result = true;
                            if ((l_lv != l_rv) && (this->_filters[i].operation == COMPARE_NOTEQUAL))
                                result = true;
                            break;
                        }
                        res = strcmp(lvalue, rvalue);
                        if( (res == 0 && this->_filters[i].operation == COMPARE_EQUAL) 
                          || (res != 0 && this->_filters[i].operation == COMPARE_NOTEQUAL))
                            result = true;
                        if (fixed_lvalue) {
                            char *tmp = (char*)malloc(1024+strlen(lvalue));
                            if(!tmp) utils::out_of_memory();
                            strcpy(tmp, lvalue);
                            this->resolve_prefix(&tmp);
                            res = strcmp(tmp, rvalue);
                            if((res == 0 && this->_filters[i].operation == COMPARE_EQUAL) || (res != 0 && this->_filters[i].operation == COMPARE_NOTEQUAL))
                                result = true;
                            free(tmp);
                        }
                        if(fixed_rvalue) {
                            char *tmp = (char*)malloc(1024+strlen(rvalue));
                            if(!tmp) utils::out_of_memory();
                            strcpy(tmp, rvalue);
                            this->resolve_prefix(&tmp);
                            res = strcmp(lvalue, tmp);
                            if((res == 0 && this->_filters[i].operation == COMPARE_EQUAL) || (res != 0 && this->_filters[i].operation == COMPARE_NOTEQUAL))
                                result = true;
                            free(tmp);
                        }
                        break;
                    case COMPARE_IEQUAL:
                    case COMPARE_INOTEQUAL:
                        res = strcmp(lv, rv);
                        if((res == 0 && this->_filters[i].operation == COMPARE_IEQUAL) 
                          || (res != 0 && this->_filters[i].operation == COMPARE_INOTEQUAL))
                            result = true;
                        if(fixed_lvalue) {
                            char *tmp = (char*)malloc(1024+strlen(lv));
                            if(!tmp) utils::out_of_memory();
                            strcpy(tmp, lv);
                            this->resolve_prefix(&tmp);
                            res = strcmp(tmp, rv);
                            if((res == 0 && this->_filters[i].operation == COMPARE_IEQUAL) || (res != 0 && this->_filters[i].operation == COMPARE_INOTEQUAL))
                                result = true;
                            free(tmp);
                        }
                        if(fixed_rvalue) {
                            char *tmp = (char*)malloc(1024+strlen(rv));
                            if(!tmp) utils::out_of_memory();
                            strcpy(tmp, rv);
                            this->resolve_prefix(&tmp);
                            res = strcmp(lv, tmp);
                            if((res == 0 && this->_filters[i].operation == COMPARE_IEQUAL) || (res != 0 && this->_filters[i].operation == COMPARE_INOTEQUAL))
                                result = true;
                            free(tmp);
                        }
                        break;
                    case COMPARE_MORE:
                        if (is_date) {
                            if (l_lv > l_rv)
                                result = true;
                            break;
                        }
                        if(atof(lvalue) > atof(rvalue))
                            result = true;
                        break;
                    case COMPARE_MOREOREQUAL:
                        if (is_date) {
                            if (l_lv >= l_rv)
                                result = true;
                            break;
                        }
                        if(atof(lvalue) >= atof(rvalue))
                            result = true;
                        break;
                    case COMPARE_LESS:
                        if (is_date) {
                            if (l_lv < l_rv)
                                result = true;
                            break;
                        }
                        if(atof(lvalue) < atof(rvalue))
                            result = true;
                        break;
                    case COMPARE_LESSOREQUAL:
                        if (is_date) {
                            if (l_lv <= l_rv)
                                result = true;
                            break;
                        }
                        if(atof(lvalue) <= atof(rvalue))
                            result = true;
                        break;
                }
                if(this->_filters[i].operation == COMPARE_IEQUAL || this->_filters[i].operation == COMPARE_INOTEQUAL || this->_filters[i].operation == COMPARE_ICONTAINS || this->_filters[i].operation == COMPARE_NOTCONTAINS) {
                    free(lv);
                    free(rv);
                }
                filter_match[i] = result;
//printf("Result of filter %i: %s\n", i, (result?"true":"false"));
            }
            // Now let us check filters combinations
//printf("filter is incomplete: %i, n_filter_groups: %i\n", incomplete, this->_n_filter_groups);
            if(!incomplete && this->_n_filter_groups > 0) {
                bool resolved;
                int iterations = 1;
                do {
//printf("Iteration %i\n", iterations);
                    resolved = true;
                    for(int i=0; i<this->_n_filter_groups; i++) {
                        if(group_match[i] != 0) continue;
                        // Check ordinary filters included in this group
                        bool one_match = false, one_not_match = false, has_filters = false;
                        for(int j=0; j<this->_n_filters; j++) {
                            if(this->_filters[j].group == i) {
                                if(filter_match[j]) one_match = true;
                                else one_not_match = true;
                                has_filters = true;
                            }
                        }
                        if(has_filters) {
                            if(this->_filter_groups[i].logic == LOGIC_OR) {
                                if(one_match)
                                    group_match[i] = 1;
                                else
                                    group_match[i] = 2;
                            }
                            else if(this->_filter_groups[i].logic == LOGIC_AND) {
                                if(one_not_match)
                                    group_match[i] = 2;
                                else if(one_match)
                                    group_match[i] = 1;
                            }
                        }
                        // Check nested filter groups
                        bool has_member_groups = false, has_undefined_member = false;
                        one_match = one_not_match = false;
                        for(int k=0; k<this->_n_filter_groups; k++) {
                            if(this->_filter_groups[k].group == i) {
                                if(group_match[k] == 1) one_match = true;
                                else if(group_match[k] == 2) one_not_match = true;
                                else has_undefined_member = true;
                                has_member_groups = true;
                            }
                        }
                        if(has_member_groups) {
                            if(has_undefined_member) group_match[i] = 0;
                            else if(this->_filter_groups[i].logic == LOGIC_OR) {
                                if(one_match || group_match[i] == 1)
                                    group_match[i] = 1;
                                else
                                    group_match[i] = 2;
                            }
                            else if(this->_filter_groups[i].logic == LOGIC_AND) {
                                if(one_not_match)
                                    group_match[i] = 2;
                                else if(one_match || group_match[i] == 1)
                                    group_match[i] = 1;
                            }
                        }
                        if(group_match[i] == 0) resolved = false;
//printf("  group %i: %i (has member groups: %i, has_undefined_member: %i)\n", i, group_match[i], has_member_groups, has_undefined_member);
                    }
                    iterations++;
                } while( !resolved && iterations < 32 );
                if(iterations == 32) {
                    char *answer = (char *)malloc(1024);
                    if(!answer) utils::out_of_memory();
                    sprintf(answer, "{\"Status\":\"Error\", \"Message\":\"Cannot resolve filters\"}", this->request_id);
                    return answer;
                }
                resolved = true;
                for(int i=0; i<this->_n_filter_groups; i++) {
                    if(this->_filter_groups[i].group == -1 && group_match[i] == 2)
                        resolved = false;
                }
                // Combination have not passed, make it incompleteto throw out on the next step
                if(!resolved)
                    this->pre_comb_value[z][0] = NULL;
            }
        }
/*
printf("\nFiltered combinations:\n");
for (int x = 0; x < this->n_pcv; x++) {
    printf("%i. ", x);
    if (this->pre_comb_value[x][0] == NULL)
        printf("(deleted)");
    for (int l = 0; l < this->n_var; l++) {
        if (this->pre_comb_value[x][l])
            printf("%s%s = %s", (l!=0?", ":""), this->var[l].obj, this->pre_comb_value[x][l]);
    }
    printf("\n\n");
}
*/
        // Output complete combinations
        char vars[1024 * N_MAX_VARIABLES], *result;
        bool first = true, first_val = true;
        result = (char *)malloc(1024 * 3 * this->n_pcv);
        if(!result) utils::out_of_memory();
        // Get rid of incomplete combinations (could be optimized)
        for (int z = 0; z < this->n_pcv; z++) {
            bool complete = true;
            for (int x = 0; x < n; x++) {
                int i = order[x];
                if (this->var[i].notexists)
                    continue;
                if (this->var[i].optional) {
                    // If this variable is optional and outside optional variables groups, it can be empty
                    if (this->var[i].optional_group == -1)
                        continue;
                    // If there is a group of optional variables, all the variables of this group can be empty
                    bool all_group_empty = true;
                    for (int y = 0; y < n; y++) {
                        if (this->var[y].optional && this->var[y].optional_group == this->var[i].optional_group && this->pre_comb_value[z][y] != NULL)
                            all_group_empty = false;
                    }
                    if (all_group_empty)
                        continue;
                }
                if (this->var[i].obj[0] != '?')
                    continue;
                if (this->pre_comb_value[z][i] == NULL) {
                    complete = false;
                    break;
                }
            }
            if (!complete) {
                free(this->pre_comb_value[z]);
                free(this->pre_comb_value_type[z]);
                free(this->pre_comb_value_lang[z]);
                if(this->n_pcv - z - 1 > 0) {
                    memmove((void*)((long)this->pre_comb_value+z*sizeof(char***)), (void*)((long)this->pre_comb_value+(z+1)*sizeof(char***)), (this->n_pcv - z - 1)*sizeof(char***) );
                    memmove((void*)((long)this->pre_comb_value_type+z*sizeof(char***)), (void*)((long)this->pre_comb_value_type+(z+1)*sizeof(char***)), (this->n_pcv - z - 1)*sizeof(char***) );
                    memmove((void*)((long)this->pre_comb_value_lang+z*sizeof(char***)), (void*)((long)this->pre_comb_value_lang+(z+1)*sizeof(char***)), (this->n_pcv - z - 1)*sizeof(char***) );
                }
                this->n_pcv--;
                z--;
            }
        }
        if(this->modifier & MOD_COUNT) {
            char *answer = (char *)malloc(1024);
            if(!answer) utils::out_of_memory();
            sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Count\":\"%i\"}", this->request_id, this->n_pcv);
            return answer;
        }
        // Sort combinations. Coarse, temporary implementation, could be optimized
        for(int j=this->_n_orders-1; j>=0; j--) {
            if(strcmp(this->_orders[j].direction, "desc") == 0) sort_order = 1;
            else sort_order = 0;
            char **unique = (char**)malloc(this->n_pcv*sizeof(char*));
            if(!unique) utils::out_of_memory();
            int *orders = (int*)malloc(this->n_pcv*sizeof(int));
            if(!orders) utils::out_of_memory();
            int sort_var_index = 0;
            for (int x = 0; x < n; x++) {
                int i = order[x];
                if (strcmp(this->var[i].obj, this->_orders[j].variable) != 0)
                    continue;
                sort_var_index = i;
            }
            for (int z = 0; z < this->n_pcv; z++) {
                if(!this->pre_comb_value[z]) continue;
                if(!this->pre_comb_value[z][sort_var_index]) {
                    this->pre_comb_value[z][sort_var_index] = (char*)malloc(2);
                    this->pre_comb_value[z][sort_var_index][0] = 0;
                }
                unique[z] = this->pre_comb_value[z][sort_var_index];
            }
            qsort(unique, this->n_pcv, sizeof(char*), utils::cstring_cmp);
            // Cycle through sorted unique items
            for (int y = 0; y < this->n_pcv; y++) {
                // Cycle through all combinations to find the next unique item
                for (int z = 0; z < this->n_pcv; z++) {
                    bool placed = false;
                    if (strcmp(this->var[sort_var_index].obj, this->_orders[j].variable) != 0)
                        continue;
                    if(strcmp(this->pre_comb_value[z][sort_var_index], unique[y]) == 0) {
                        bool found = false;
                        for(int v=0; v<y; v++) {
                            if(orders[v] == z) {
                                found = true;
                                break;
                            }
                        }
                        if(!found) {
                            orders[y] = z;
                            placed = true;
                        }
                    }
                    if(placed) break;
                }
            }
            char ***new_pre_comb_value = (char ***)malloc(this->n_pcv * sizeof(char **));
            char ***new_pre_comb_value_type = (char ***)malloc(this->n_pcv * sizeof(char **));
            char ***new_pre_comb_value_lang = (char ***)malloc(this->n_pcv * sizeof(char **));
            if(!new_pre_comb_value) utils::out_of_memory();
            if(!new_pre_comb_value_type) utils::out_of_memory();
            if(!new_pre_comb_value_lang) utils::out_of_memory();
            for (int y = 0; y < this->n_pcv; y++) {
                new_pre_comb_value[y] = this->pre_comb_value[orders[y]];
                new_pre_comb_value_type[y] = this->pre_comb_value_type[orders[y]];
                new_pre_comb_value_lang[y] = this->pre_comb_value_lang[orders[y]];
            }
            memcpy(this->pre_comb_value, new_pre_comb_value, this->n_pcv * sizeof(char **));
            memcpy(this->pre_comb_value_type, new_pre_comb_value_type, this->n_pcv * sizeof(char **));
            memcpy(this->pre_comb_value_lang, new_pre_comb_value_lang, this->n_pcv * sizeof(char **));
            free(new_pre_comb_value); free(new_pre_comb_value_type); free(new_pre_comb_value_lang);
            free(unique); free(orders);
        }

        // Response output
        sprintf(vars, "\"Vars\": [");
        sprintf(result, "\"Result\": [");
        for (int x = 0; x < n; x++)
        {
            int i = order[x];
            if (this->var[i].notexists)
                continue;
            if (this->var[i].obj[0] != '?')
                continue;
            if (!first)
                strcat(vars, ", ");
            first = false;
            sprintf((char *)((unsigned long)vars + strlen(vars)), "\"%s\"", this->var[i].obj);
        }
        strcat(vars, "] ");
        first = true;
        int sent = 0;
        for (int z = 0; z < this->n_pcv; z++) {
            if (this->offset != -1 && z < this->offset) continue;
            if (this->limit != -1 && sent >= this->limit) break;
            sent++;
            if (!first)
                strcat(result, ", ");
            first = false;
            first_val = true;
            strcat(result, " [");
            for (int x = 0; x < n; x++) {
                int i = order[x];
                if (this->var[i].notexists)
                    continue;
                if (this->var[i].obj[0] != '?')
                    continue;
                if (!first_val)
                    strcat(result, ", ");
                first_val = false;
                if (this->pre_comb_value[z][i] == NULL)
                    sprintf((char *)((unsigned long)result + strlen(result)), "[\"\", \"\", \"\"]");
                else
                    sprintf((char *)((unsigned long)result + strlen(result)), "[\"%s\", \"%s\", \"%s\"]", this->pre_comb_value[z][i], this->pre_comb_value_type[z][i] ? this->pre_comb_value_type[z][i] : "", this->pre_comb_value_lang[z][i] ? this->pre_comb_value_lang[z][i] : "");
            }
            strcat(result, "]");
        }
        strcat(result, "] ");
        utils::logger(LOG, "(child) Return triples chain", "", this->n_pcv);

        char *answer = (char *)malloc(1024 + strlen(vars) + strlen(result));
        if(!answer) utils::out_of_memory();
        sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", %s, %s}", reqid, vars, result);
        free(result);
        return answer;
    }