#include <stdio.h>
#include "cachelab.h"
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

typedef struct {
    int tag;
    int valid;
    int LRU;
}Line;

typedef struct {
    Line *lines;
}Set;

typedef struct {
    int n_line;
    int n_set;
    Set *sets;
}Cache;

int s = 0;
int b = 0;
int t = 0;
int n_hit = 0;
int n_miss = 0;
int n_evict = 0;

int E,S;
int address;
Cache cache = {};

void update(Cache cache) {
    int max_idx = 0;
    int max_LRU = 0;
    unsigned idx = ((-1UL)>>(64-s))&(address>>b);
    unsigned tag = address>>(s+b);

    for (int i = 0; i < cache.n_line; i+=1) {
        if (cache.sets[idx].lines[i].valid) {
            (cache.sets[idx].lines[i].LRU) += 1;
        }
        else {
            cache.sets[idx].lines[i].LRU = 0;
        }
    }
    for (int i = 0; i < cache.n_line; i+=1) {
        if ((cache.sets[idx].lines[i].valid) && (cache.sets[idx].lines[i].tag == tag)){
            cache.sets[idx].lines[i].LRU = 0;
            n_hit+=1;
            return;
        }
    }
    n_miss+=1;
    for (int i = 0; i < cache.n_line; i+=1) {
        if (!cache.sets[idx].lines[i].valid) {
            cache.sets[idx].lines[i].tag = tag;
            cache.sets[idx].lines[i].valid = 1;
            cache.sets[idx].lines[i].LRU = 0;
            return;
        }
    }
    n_evict+=1;
    for (int i = 0; i < cache.n_line; i+=1) {
        if (cache.sets[idx].lines[i].LRU > max_LRU) {
            max_idx = i;
            max_LRU = cache.sets[idx].lines[i].LRU;
        }
    }
    cache.sets[idx].lines[max_idx].tag = tag;
    cache.sets[idx].lines[max_idx].LRU = 0;
    return;
}

int main(int argc, char *argv[])
{
    char type;
    int opt;
    FILE *fp = 0;

    while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
	switch(opt){
            case 's':
		s = atoi(optarg);
		cache.n_set = 2 << s;
		break;
	    case 'E':
		cache.n_line = atoi(optarg);
		break;
	    case 'b':
		b = atoi(optarg);
		break;
	    case 't':
		if (!(fp = fopen(optarg, "r"))) {
                    return 1;
                }
		break;
	    default:
		return 1;
	    }
    }
	
    if (fp == NULL)
    {
        fprintf(stderr, "File is wrong!\n");
        exit(-1);
    }
    
    E = cache.n_line;
    S = cache.n_set;
    t = 32 - (s + b);
   
    if (s == 0 || E == 0 || b == 0 || t == 0) { 
        printf("wrong cache setting");
        return 1;
    }

    cache.sets = malloc(sizeof(Set) * S);
    for (int i = 0; i < S; i+=1) {
	cache.sets[i].lines = malloc(sizeof(Line) * E);
    }

    while (fscanf(fp, " %c %x%*c%*d", &type, &address) >0 ) {
	if (type == 'I') {
            continue;
        }
	else if (type == 'M') {
            update(cache);
        }
        update(cache);
    }

    for (int i = 0; i < S; i+=1) {
    	free(cache.sets[i].lines);
    }
    free(cache.sets);
     fclose(fp);
    
    printSummary(n_hit, n_miss, n_evict);
    return 0;
}
