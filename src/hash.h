/************************************************************************
 hash.h
 ************************************************************************/
#ifndef HASH_H
#define HASH_H


typedef unsigned int HASHKEYTYPE;
typedef struct HASHTABLE_T *HASHTABLE;
typedef int BOOL;

#define FALSE 0
#define TRUE  1

extern void			hash_usage		(void);
extern BOOL			hash_test		(void);
extern HASHTABLE	hash_create		(unsigned long InitialSize, BOOL KeyIsString);
extern HASHTABLE	hash_destroy	(HASHTABLE table);
extern BOOL			hash_add		(HASHTABLE table, HASHKEYTYPE Key, const void *Data);
extern BOOL			hash_addstring	(HASHTABLE table, char * Key, const void * Data);
extern void *		hash_remove		(HASHTABLE table, HASHKEYTYPE Key);
extern void *		hash_lookup		(HASHTABLE table, HASHKEYTYPE Key);
extern size_t		hash_count		(HASHTABLE table);
extern void *		hash_search		(HASHTABLE table, BOOL (*Search)(HASHKEYTYPE key, void *, va_list), ...);
extern void *		hash_searchwithvalist(HASHTABLE table, BOOL (*Search)(HASHKEYTYPE key, void *, va_list), va_list args);
extern BOOL			hash_exists		(HASHTABLE table, HASHKEYTYPE Key);


#endif
