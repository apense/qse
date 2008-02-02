/*
 * $Id: map.c,v 1.11 2007/11/10 15:21:40 bacon Exp $
 *
 * {License}
 */

#include <ase/awk/awk_i.h>

static ase_size_t hashkey (const ase_char_t* keyptr, ase_size_t keylen);
static int rehash (ase_awk_map_t* map);

#define FREE_PAIR(map,pair) \
	do { \
		ASE_AWK_FREE ((map)->awk, (ase_char_t*)ASE_AWK_PAIR_KEYPTR(pair)); \
		if ((map)->freeval != ASE_NULL) \
			(map)->freeval ((map)->owner, ASE_AWK_PAIR_VAL(pair)); \
		ASE_AWK_FREE ((map)->awk, pair); \
	} while (0)

ase_awk_map_t* ase_awk_map_open (
	void* owner, ase_size_t capa, unsigned int factor,
	void(*freeval)(void*,void*), void(*sameval)(void*,void*), 
	ase_awk_t* awk)
{
	ase_awk_map_t* map;

	ASE_ASSERTX (capa > 0, "the initial capacity should be greater than 0");

	map = (ase_awk_map_t*) ASE_AWK_MALLOC (
		awk, ASE_SIZEOF(ase_awk_map_t));
	if (map == ASE_NULL) return ASE_NULL;

	map->awk = awk;
	map->buck = (ase_awk_pair_t**) 
		ASE_AWK_MALLOC (awk, ASE_SIZEOF(ase_awk_pair_t*) * capa);
	if (map->buck == ASE_NULL) 
	{
		ASE_AWK_FREE (awk, map);
		return ASE_NULL;	
	}

	map->owner = owner;
	map->capa = capa;
	map->size = 0;
	map->freeval = freeval;
	map->sameval = sameval;
	while (capa > 0) map->buck[--capa] = ASE_NULL;

	map->factor = factor;
	map->threshold = ((ase_size_t)map->factor) * map->capa / 100;

	return map;
}

void ase_awk_map_close (ase_awk_map_t* map)
{
	ase_awk_map_clear (map);
	ASE_AWK_FREE (map->awk, map->buck);
	ASE_AWK_FREE (map->awk, map);
}

void ase_awk_map_clear (ase_awk_map_t* map)
{
	ase_size_t i;
	ase_awk_pair_t* pair, * next;

	for (i = 0; i < map->capa; i++) 
	{
		pair = map->buck[i];

		while (pair != ASE_NULL) 
		{
			next = ASE_AWK_PAIR_LNK(pair);
			FREE_PAIR (map, pair);
			map->size--;
			pair = next;
		}

		map->buck[i] = ASE_NULL;
	}
}

ase_size_t ase_awk_map_getsize (ase_awk_map_t* map)
{
	return map->size;
}

ase_awk_pair_t* ase_awk_map_get (
	ase_awk_map_t* map, const ase_char_t* keyptr, ase_size_t keylen)
{
	ase_awk_pair_t* pair;
	ase_size_t hc;

	hc = hashkey(keyptr,keylen) % map->capa;
	pair = map->buck[hc];

	while (pair != ASE_NULL) 
	{
		if (ase_strxncmp (
			ASE_AWK_PAIR_KEYPTR(pair), ASE_AWK_PAIR_KEYLEN(pair), 
			keyptr, keylen) == 0) return pair;

		pair = ASE_AWK_PAIR_LNK(pair);
	}

	return ASE_NULL;
}

ase_awk_pair_t* ase_awk_map_put (
	ase_awk_map_t* map, const ase_char_t* keyptr, ase_size_t keylen, 
	void* val)
{
	int n;
	ase_awk_pair_t* px;

	n = ase_awk_map_putx (map, keyptr, keylen, val, &px);
	if (n < 0) return ASE_NULL;
	return px;
}

int ase_awk_map_putx (
	ase_awk_map_t* map, const ase_char_t* keyptr, ase_size_t keylen, 
	void* val, ase_awk_pair_t** px)
{
	ase_awk_pair_t* pair;
	ase_size_t hc;

	hc = hashkey(keyptr,keylen) % map->capa;
	pair = map->buck[hc];

	while (pair != ASE_NULL) 
	{
		if (ase_strxncmp (
			ASE_AWK_PAIR_KEYPTR(pair), ASE_AWK_PAIR_KEYLEN(pair), 
			keyptr, keylen) == 0) 
		{
			if (px != ASE_NULL)
				*px = ase_awk_map_setpair (map, pair, val);
			else
				ase_awk_map_setpair (map, pair, val);

			return 0; /* value changed for the existing key */
		}
		pair = ASE_AWK_PAIR_LNK(pair);
	}

	if (map->threshold > 0 && 
	    map->size >= map->threshold)
	{
		if (rehash(map) == 0) /* ignore the rehash error */
		{
			hc = hashkey(keyptr,keylen) % map->capa;
		}
	}

	pair = (ase_awk_pair_t*) ASE_AWK_MALLOC (
		map->awk, ASE_SIZEOF(ase_awk_pair_t));
	if (pair == ASE_NULL) return -1; /* error */

	/* duplicate the key if it is new */
	ASE_AWK_PAIR_KEYPTR(pair) = ase_awk_strxdup (map->awk, keyptr, keylen);
	if (ASE_AWK_PAIR_KEYPTR(pair) == ASE_NULL)
	{
		ASE_AWK_FREE (map->awk, pair);
		return -1; /* error */
	}

	ASE_AWK_PAIR_KEYLEN(pair) = keylen;
	ASE_AWK_PAIR_VAL(pair) = val;
	ASE_AWK_PAIR_LNK(pair) = map->buck[hc];
	map->buck[hc] = pair;
	map->size++;

	if (px != ASE_NULL) *px = pair;
	return 1; /* new key added */
}

ase_awk_pair_t* ase_awk_map_set (
	ase_awk_map_t* map, const ase_char_t* keyptr, ase_size_t keylen, 
	void* val)
{
	ase_awk_pair_t* pair;
	ase_size_t hc;

	hc = hashkey(keyptr,keylen) % map->capa;
	pair = map->buck[hc];

	while (pair != ASE_NULL) 
	{
		if (ase_strxncmp (
			ASE_AWK_PAIR_KEYPTR(pair), ASE_AWK_PAIR_KEYLEN(pair), 
			keyptr, keylen) == 0) 
		{
			return ase_awk_map_setpair (map, pair, val);
		}
		pair = ASE_AWK_PAIR_LNK(pair);
	}

	return ASE_NULL;
}

ase_awk_pair_t* ase_awk_map_getpair (
	ase_awk_map_t* map, const ase_char_t* keyptr, ase_size_t keylen, 
	void** val)
{
	ase_awk_pair_t* pair;

	pair = ase_awk_map_get (map, keyptr, keylen);
	if (pair == ASE_NULL) return ASE_NULL; 
	*val = ASE_AWK_PAIR_VAL(pair);

	return pair;
}

ase_awk_pair_t* ase_awk_map_setpair (
	ase_awk_map_t* map, ase_awk_pair_t* pair, void* val)
{
	/* use this function with care */
	if (ASE_AWK_PAIR_VAL(pair) == val) 
	{
		/* if the old value and the new value are the same,
		 * it just calls the handler for this condition. 
		 * No value replacement occurs. */
		if (map->sameval != ASE_NULL)
		{
			map->sameval (map->owner, val);
		}
	}
	else
	{
		/* frees the old value */
		if (map->freeval != ASE_NULL) 
		{
			map->freeval (map->owner, ASE_AWK_PAIR_VAL(pair));
		}

		/* the new value takes the place */
		ASE_AWK_PAIR_VAL(pair) = val;
	}


	return pair;
}

int ase_awk_map_remove (
	ase_awk_map_t* map, const ase_char_t* keyptr, ase_size_t keylen)
{
	ase_awk_pair_t* pair, * prev;
	ase_size_t hc;

	hc = hashkey(keyptr,keylen) % map->capa;
	pair = map->buck[hc];
	prev = ASE_NULL;

	while (pair != ASE_NULL) 
	{
		if (ase_strxncmp (
			ASE_AWK_PAIR_KEYPTR(pair), ASE_AWK_PAIR_KEYLEN(pair), 
			keyptr, keylen) == 0) 
		{
			if (prev == ASE_NULL) 
				map->buck[hc] = ASE_AWK_PAIR_LNK(pair);
			else prev->next = ASE_AWK_PAIR_LNK(pair);

			FREE_PAIR (map, pair);
			map->size--;

			return 0;
		}

		prev = pair;
		pair = ASE_AWK_PAIR_LNK(pair);
	}

	return -1;
}

int ase_awk_map_walk (ase_awk_map_t* map, 
	int (*walker) (ase_awk_pair_t*,void*), void* arg)
{
	ase_size_t i;
	ase_awk_pair_t* pair, * next;

	for (i = 0; i < map->capa; i++) 
	{
		pair = map->buck[i];

		while (pair != ASE_NULL) 
		{
			next = ASE_AWK_PAIR_LNK(pair);
			if (walker(pair,arg) == -1) return -1;
			pair = next;
		}
	}

	return 0;
}

ase_awk_pair_t* ase_awk_map_getfirstpair (
	ase_awk_map_t* map, ase_size_t* buckno)
{
	ase_size_t i;
	ase_awk_pair_t* pair;

	for (i = 0; i < map->capa; i++)
	{
		pair = map->buck[i];
		if (pair != ASE_NULL) 
		{
			*buckno = i;
			return pair;
		}
	}

	return ASE_NULL;
}

ase_awk_pair_t* ase_awk_map_getnextpair (
	ase_awk_map_t* map, ase_awk_pair_t* pair, ase_size_t* buckno)
{
	ase_size_t i;
	ase_awk_pair_t* next;

	next = ASE_AWK_PAIR_LNK(pair);
	if (next != ASE_NULL) 
	{
		/* no change in bucket number */
		return next;
	}

	for (i = (*buckno)+1; i < map->capa; i++)
	{
		pair = map->buck[i];
		if (pair != ASE_NULL) 
		{
			*buckno = i;
			return pair;
		}
	}

	return ASE_NULL;
}


static ase_size_t hashkey (const ase_char_t* keyptr, ase_size_t keylen)
{
	ase_size_t n = 0, i;
	const ase_char_t* end = keyptr + keylen;

	while (keyptr < end)
	{
		ase_byte_t* bp = (ase_byte_t*)keyptr;
		for (i = 0; i < ASE_SIZEOF(*keyptr); i++) n = n * 31 + *bp++;
		keyptr++;
	}	

	return n;
}

static int rehash (ase_awk_map_t* map)
{
	ase_size_t i, hc, new_capa;
	ase_awk_pair_t** new_buck;

	new_capa = (map->capa >= 65536)? (map->capa + 65536): (map->capa << 1);

	new_buck = (ase_awk_pair_t**) ASE_AWK_MALLOC (
		map->awk, ASE_SIZEOF(ase_awk_pair_t*) * new_capa);
	if (new_buck == ASE_NULL) 
	{
		/* once rehash fails, the rehashing is disabled */
		map->threshold = 0;
		return -1;
	}

	for (i = 0; i < new_capa; i++) new_buck[i] = ASE_NULL;

	for (i = 0; i < map->capa; i++)
	{
		ase_awk_pair_t* pair = map->buck[i];

		while (pair != ASE_NULL) 
		{
			ase_awk_pair_t* next = ASE_AWK_PAIR_LNK(pair);

			hc = hashkey(
				ASE_AWK_PAIR_KEYPTR(pair),
				ASE_AWK_PAIR_KEYLEN(pair)) % new_capa;

			ASE_AWK_PAIR_LNK(pair) = new_buck[hc];
			new_buck[hc] = pair;

			pair = next;
		}
	}

	ASE_AWK_FREE (map->awk, map->buck);
	map->buck = new_buck;
	map->capa = new_capa;
	map->threshold = ((ase_size_t)map->factor) * map->capa / 100;

	return 0;
}