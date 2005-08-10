/************************************************************************
 hash.c - This module maintains a hash table of key/value pairs.
		  Keys can be strings of any size, or numbers up to size 
		  unsigned long (HASHKEYTYPE).
		  Values should be a pointer to some data you wish to store.		
		  See hash_usage() for an example of use.
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "hash.h"

// Enums
typedef enum  {FindEmpty = 0, FindExisting = 1} enumFind;

// Structs
struct ROW
{
	HASHKEYTYPE	Key;
	int			Type;
	void *		Data;
};

struct HASHTABLE_T
{
	int 		   KeyIsString;
	unsigned long  MaxLoad;
	unsigned long  MaxRows;
	unsigned long  ItemCount;
	struct ROW	  *Rows;
};

// Function prototypes
static void hash_expand(HASHTABLE OldTable);

// Current module name (used by error processing)
static char *module = "hash";

// Error list
static const char *ERR_MUST_CREATE_HASH_TABLE_FIRST = "You must create the hash table before using it!";
static const char *ERR_NOT_STRING_TABLE = "Not a string hash table!";

// Deleted entry indicator
static const int Empty   = 0x00000000;
static const int InUse   = 0x11111111;
static const int Deleted = 0xffffffff;

// Expand the table when the usage exceeds this amount (80%)
static double LoadFactor = 0.8;


/************************************************************************
 ExitEarly - Quit the program early
 ************************************************************************/
static void ExitEarly(const char *strModule, const char *strFunction, const char *errmsg, ...)
{
	va_list args;
	fprintf(stderr, "Error in module %s, function %s: ", strModule, strFunction);
	va_start(args, errmsg);
	vfprintf(stderr, errmsg, args);
	va_end(args);
	exit(1);
}

/************************************************************************
 hash1 - Calculate a hash value for the desired key
 ************************************************************************/
static HASHKEYTYPE hash1(HASHKEYTYPE Key, BOOL KeyIsString)
{

	if (!KeyIsString)
	{
		return Key;
	}
	else
	{
		unsigned char *s = (unsigned char *)Key;
		HASHKEYTYPE    h = 0;
 		while (*s) 
			h = h * 31UL + *s++;
		return h ;
	}
}

/************************************************************************
 hash2 - Calculate a secondary hash value for the desired key
 ************************************************************************/
static HASHKEYTYPE hash2(HASHKEYTYPE Key, BOOL KeyIsString)
{
	return hash1(Key, KeyIsString) >> 3;
}

/************************************************************************
 NextPrime - Return the next prime number past a certain number.
 ************************************************************************/
static unsigned long NextPrime(unsigned long NumberDesired) 
{
	unsigned long  i;
	unsigned long  HalfwayPoint;
	int 		   IsDivisible ;

	do
	{
		NumberDesired++;
		IsDivisible = FALSE;
		HalfwayPoint = NumberDesired / 2;
		for (i = 2; i <= HalfwayPoint; i++)
		{
			if (NumberDesired % i == 0)
			{
				IsDivisible = TRUE;
				break;
			}
		}
	} while (IsDivisible);

	return NumberDesired;

};

/************************************************************************
 hash_create - Create a new hash table 
 ************************************************************************/
HASHTABLE hash_create(unsigned long InitialSize, BOOL KeyIsString)
{
	HASHTABLE table;

	// Allocate space for the hash table.
	table = malloc(sizeof(*table));
	memset( table, 0,  sizeof(*table) );

	// Minimum size of hash table is 8.
	if (InitialSize < 8 )
		InitialSize = 8;

	// Allocate it large enough so that it's not more than 80% full.
	InitialSize = (unsigned long)(InitialSize * 1.25);

	// Allocate space for the rows in the hash table
	InitialSize 	   = NextPrime(InitialSize);
	table->MaxLoad	   = (unsigned long)(InitialSize * LoadFactor);
	table->MaxRows	   = InitialSize;
	table->Rows 	   = malloc(InitialSize * sizeof(struct ROW));
	table->KeyIsString = KeyIsString;

	memset( table->Rows, 0,  InitialSize * sizeof(struct ROW) );


	return table;

}


/************************************************************************
 hash_destroy - Destroy a hash table.
 ************************************************************************/
HASHTABLE hash_destroy(HASHTABLE table) 
{
	char *function = "hash_destroy";

	// Make sure the table is valid
	if (table == NULL)
		ExitEarly(module, function, ERR_MUST_CREATE_HASH_TABLE_FIRST);

	// Free the rows in the table
	free(table->Rows);

	// Free the table itself
	free(table);

	// Return NULL
	return NULL;
}


/************************************************************************
 FindSlot - Find a slot in the hash table, either empty or existing.
 ************************************************************************/
static int FindSlot(HASHTABLE table, HASHKEYTYPE Key, int FindMethod, HASHKEYTYPE *Slot)
{
	char		  *function = "FindSlot";
	HASHKEYTYPE    hash 	= 0;
	unsigned long  i;
	HASHKEYTYPE    AddlAmt	= 0;

	// Hash the key.
	hash = (HASHKEYTYPE)(hash1(Key, table->KeyIsString) % table->MaxRows);

	// Perform the lookup a maximum of MaxRows times.
	for (i = 0; i < table->MaxRows; i++) 
	{
		// Are we supposed to find an empty slot or look for an existing key?
		if (FindMethod == FindExisting)
		{
			// Look for an existing key
			if (table->Rows[hash].Type == Empty)
			{
				// Couldn't find the key
				return FALSE;
			}
			else if (table->Rows[hash].Type == InUse &&
					 ( (table->KeyIsString && strcmp((char *)table->Rows[hash].Key, (char *)Key) == 0) ||
					   (!table->KeyIsString && table->Rows[hash].Key == Key) ) )
				
			{
				// Found the key
				*Slot = hash;
				return TRUE;
			}
		}
		else
		{
			// Look for an empty slot to insert the new key into
			if (table->Rows[hash].Type != InUse)
			{
				// Found a spot
				*Slot = hash;
				return TRUE;
			}
			else if ( (table->KeyIsString && strcmp((char *)table->Rows[hash].Key, (char *)Key) == 0) ||
					  (!table->KeyIsString && table->Rows[hash].Key == Key)  )
			{
				// Key already exists!
				return FALSE;
			}
		}

		// Rehash the hash and try again.
		if (AddlAmt == 0)
			AddlAmt = (HASHKEYTYPE)(hash2(Key, table->KeyIsString) % (table->MaxRows >> 3) + 1);
		hash = (hash + AddlAmt) % table->MaxRows;

	}

	// Searched the whole table unsuccessfully! :-O
	// Should never hit this point, because the table expands when it gets too full.
	ExitEarly(module, function, "Findslot failed!");
	return FALSE;
}


/************************************************************************
 hash_add - Add an item to a hash table.
 ************************************************************************/
BOOL hash_add(HASHTABLE table, HASHKEYTYPE Key, const void *Data) 
{
	char		  *function = "hash_add";
	HASHKEYTYPE    position;

	if (table == NULL)
		ExitEarly(module, function, ERR_MUST_CREATE_HASH_TABLE_FIRST);

	// Do not let the table become overloaded
	if (table->ItemCount > table->MaxLoad)
		hash_expand(table);
 
	// Search for an empty slot
	if (FindSlot(table, Key, FindEmpty, &position))
	{
		// Insert the data into the table.
		table->Rows[position].Key  = Key;
		table->Rows[position].Type = InUse;
		table->Rows[position].Data = (void *)Data;
		table->ItemCount++;
		return TRUE;
	}
	else
	{
		// The entry already exists!
		return FALSE;
	}
}


/************************************************************************
 hash_addstring - Add an string to a hash table.
 ************************************************************************/
BOOL hash_addstring(HASHTABLE table, char * Key, const void * Data) 
{
	char		  *function = "hash_addstring";

	if (table == NULL)
		ExitEarly(module, function, ERR_MUST_CREATE_HASH_TABLE_FIRST);
	if (table->KeyIsString == FALSE)
		ExitEarly(module, function, ERR_NOT_STRING_TABLE);
	return hash_add(table, (HASHKEYTYPE)Key, Data);
}

/************************************************************************
 hash_remove - Remove an item from a hash table.
 ************************************************************************/
void *hash_remove(HASHTABLE table, HASHKEYTYPE Key) 
{
	char		  *function = "hash_remove";
	void		  *Data;
	unsigned int   position;

	if (table == NULL)
		ExitEarly(module, function, ERR_MUST_CREATE_HASH_TABLE_FIRST);
 
	if (FindSlot(table, Key, FindExisting, &position))
	{
		// Found the item, now mark it as deleted.
		Data = table->Rows[position].Data;
		table->Rows[position].Key  = 0;
		table->Rows[position].Type = Deleted;
		table->Rows[position].Data = 0;
		table->ItemCount--;
		return Data;
	}
	else
	{
		// Couldn't find the item to remove it.
		return NULL;
	}
}

/************************************************************************
 hash_lookup - Lookup an item from a hash table.
 ************************************************************************/
void *hash_lookup(HASHTABLE table, HASHKEYTYPE Key) 
{
	char		  *function = "hash_lookup";
	HASHKEYTYPE    position;

	if (table == NULL)
		ExitEarly(module, function, ERR_MUST_CREATE_HASH_TABLE_FIRST);

	if (FindSlot(table, Key, FindExisting, &position))
		return table->Rows[position].Data;
	else
		return NULL;
}

/************************************************************************
 hash_exists - Returns whether or not a key exists in the table.
 ************************************************************************/
BOOL hash_exists(HASHTABLE table, HASHKEYTYPE Key) 
{
	char		  *function = "hash_lookup";
	HASHKEYTYPE    position;

	if (table == NULL)
		ExitEarly(module, function, ERR_MUST_CREATE_HASH_TABLE_FIRST);

	if (FindSlot(table, Key, FindExisting, &position))
		return TRUE;
	else
		return FALSE;
}

/************************************************************************
 hash_count - Return a count of all items in the hash table
 ************************************************************************/
extern size_t hash_count(HASHTABLE table)
{
	char *function = "hash_count";
	if (table == NULL)
		ExitEarly(module, function, ERR_MUST_CREATE_HASH_TABLE_FIRST);
 
	return table->ItemCount;
}

/************************************************************************
 hash_expand - Expand the hash table to accommodate more entries.
 ************************************************************************/
static void hash_expand(HASHTABLE OldTable)
{
	HASHTABLE		NewTable;
	unsigned long	i;

	// Create a new temporary table
	NewTable = hash_create(OldTable->MaxRows * 2, OldTable->KeyIsString);

	// Add the data from the old table into the new table
	for (i = 0; i < OldTable->MaxRows; i++)
	{
		if (OldTable->Rows[i].Type == InUse)
			hash_add(NewTable, OldTable->Rows[i].Key, OldTable->Rows[i].Data);
	}

	// Free the old table rows
	free(OldTable->Rows);

	// Overlay the old table values with the temporary table values.
	OldTable->MaxRows	= NewTable->MaxRows;
	OldTable->MaxLoad	= NewTable->MaxLoad;
	OldTable->ItemCount = NewTable->ItemCount;
	OldTable->Rows		= NewTable->Rows;

	// Destroy the temporary table
	NewTable->Rows = NULL;
	free(NewTable);
}

/************************************************************************
 hash_search - Search for an item in the hashtable.
 ************************************************************************/
extern void *hash_search(HASHTABLE table, BOOL (*Search)(HASHKEYTYPE key, void *, va_list), ...)
{
	va_list args;
	unsigned int i;
	void *p = NULL;

	va_start(args, Search);
	for (i = 0; i < table->MaxRows; i++)
	{
		if (table->Rows[i].Type != Deleted &&
		    table->Rows[i].Type != Empty)
		{
			if (Search(table->Rows[i].Key, table->Rows[i].Data, args) != FALSE)
			{
				p = table->Rows[i].Data;
				break;
			}
		}
	}
	va_end(args);
	return p;
}
	
/************************************************************************
 hash_searchwithvalist - Search for an item in the hashtable.
 A va_list has already been passed in, no need to extract one.
 ************************************************************************/
extern void *hash_searchwithvalist(HASHTABLE table, BOOL (*Search)(HASHKEYTYPE key, void *, va_list), va_list args)
{
	unsigned int i;
	void *p = NULL;

	for (i = 0; i < table->MaxRows; i++)
	{
		if (table->Rows[i].Type != Deleted &&
		    table->Rows[i].Type != Empty)
		{
			if (Search(table->Rows[i].Key, table->Rows[i].Data, args) != FALSE)
			{
				p = table->Rows[i].Data;
				break;
			}
		}
	}
	return p;
}
	
/************************************************************************
 * hash_usage - Example of how to use this module.
 ************************************************************************/
struct person_t
{
	int  ID;
	char Name[32];
	int  Age;
};

extern void hash_usage(void)
{
	void			*table;
	struct person_t *p;
	struct person_t *lookup;
	char			*StringKey = "Lloyd";
	int 			 NumberKey = 1234567;
	
	// Create some data to store
	p		= malloc(sizeof(*p));
	memset ( p, 0 ,sizeof( *p));
	p->ID	= NumberKey;
	p->Age	= 20;
	sprintf(p->Name, StringKey);

	// Numeric hash example

	// Create the hash table
	table = hash_create(10, FALSE);

	// Add a key/value pair to the table
	if (hash_add(table, p->ID, p) == FALSE)
		printf("Entry already exists!");

	// Lookup a key/value pair in the table
	if ((lookup = hash_lookup(table, NumberKey)) == FALSE)
		printf("Entry not found!");
	else
		printf("Person found by ID: %d. Name is: %s\n", lookup->ID, lookup->Name);
	
	// Remove a key/value pair from the table
	if ((lookup = hash_remove(table, NumberKey)) == FALSE)
		printf("Entry not found!");
	else
		// you COULD free lookup here, but we're not done with it.
		;

	// Free the table.
	table = hash_destroy(table);
	

	// String hash example

	// Create the hash table
	table = hash_create(10, TRUE);

	// Add a key/value pair to the table
	if (hash_add(table, (HASHKEYTYPE)p->Name, p) == FALSE)
		printf("Entry already exists!");

	// Lookup a key/value pair in the table
	if ((lookup = hash_lookup(table, (HASHKEYTYPE)StringKey)) == FALSE)
		printf("Entry not found!");
	else
		printf("Person found by name: %s. ID is: %d\n", lookup->Name, lookup->ID);

	// Remove a key/value pair from the table
	if ((lookup = hash_remove(table, (HASHKEYTYPE)StringKey)) == FALSE)
		printf("Entry not found!");
	else
		free(lookup);

	// Free the table.
	table = hash_destroy(table);
}

/************************************************************************
 hash_test - Test the hash table.
 ************************************************************************/
static BOOL Search(HASHKEYTYPE key, void *p, va_list args)
{
	char *s = p;
	char *find = va_arg(args, char *);
	if (strcmp(s, find) == 0)
		return TRUE;
	else
		return FALSE;
}

static BOOL Iterate(HASHKEYTYPE key, void *p, va_list args)
{
	// Just iterate a counter.
	unsigned int *i = va_arg(args, unsigned int *);
	*i = *i + 1;
	// Return false to keep searching.
	return FALSE;
}

extern BOOL hash_test(void)
{
	HASHTABLE table;
	HASHTABLE strings;
	int i;
	int max = 1000000;
	clock_t start;
	char **s;
	char temp[80];
	char *fmt = "\t%-8s %4d milliseconds, %8d per second.\n";
	unsigned int ctr = 0;

	// Logical tests
	table = hash_create(8, FALSE);
	if (hash_add(table, 0, &table) == FALSE)			return FALSE;
	if (hash_add(table, 1, &table) == FALSE)			return FALSE;
	if (hash_add(table, 0xffffffff, &table) == FALSE)	return FALSE;

	strings = hash_create(8, TRUE);
	if (hash_add(strings, (HASHKEYTYPE)"test1", &strings) == FALSE)	return FALSE;
	if (hash_add(strings, (HASHKEYTYPE)"test2", &strings) == FALSE)	return FALSE;
	if (hash_add(strings, (HASHKEYTYPE)"test3", &strings) == FALSE)	return FALSE;

	if (hash_lookup(table, 0) != &table)				return FALSE;
	if (hash_lookup(table, 1) != &table)				return FALSE;
	if (hash_lookup(table, 0xffffffff) != &table)		return FALSE;

	if (hash_lookup(strings, (HASHKEYTYPE)"test1") != &strings)		return FALSE;
	if (hash_lookup(strings, (HASHKEYTYPE)"test2") != &strings)		return FALSE;
	if (hash_lookup(strings, (HASHKEYTYPE)"test3") != &strings)		return FALSE;

	if (hash_remove(strings, (HASHKEYTYPE)"test1") != &strings)     return FALSE;
	if (hash_lookup(strings, (HASHKEYTYPE)"test1") != FALSE)        return FALSE;

	if (hash_destroy(table) != NULL)					return FALSE;
	if (hash_destroy(strings) != NULL)					return FALSE;


	// Stress tests for numbers
	printf("HASH Numeric Tests for %d items\n", max);
	table = hash_create(max, FALSE);

	start = clock();
	for (i = 0; i < max; i++)
		hash_add(table, i, &table);
	printf(fmt, "Adds:", clock() - start, (max * CLOCKS_PER_SEC)/(clock() - start));

	start = clock();
	for (i = 0; i < max; i++)
		if (hash_lookup(table, i) != &table)
			break;
	printf(fmt, "Lookups:", clock() - start, (max * CLOCKS_PER_SEC)/(clock() - start));

	start = clock();
	for (i = 0; i < max; i++)
		if (hash_remove(table, i) != &table)
			break;
	printf(fmt, "Deletes:", clock() - start, (max * CLOCKS_PER_SEC)/(clock() - start));
	if (hash_count(table) != 0)
		return FALSE;
	table = hash_destroy(table);

	// Stress tests for strings
	printf("HASH strings Tests for %d items\n", max);
	table = hash_create(max, TRUE);
	s = malloc(sizeof(*s) * max);
	memset ( s, 0 ,sizeof( *s)*max);
	
	for (i = 0; i < max; i++)
	{
		sprintf(temp, "Item %d", i);
		s[i] = strdup(temp);
	}

	start = clock();
	for (i = 0; i < max; i++)
		hash_add(table, (HASHKEYTYPE)s[i], s[i]);
	printf(fmt, "Adds:", clock() - start, (max * CLOCKS_PER_SEC)/(clock() - start));

	start = clock();
	for (i = 0; i < max; i++)
		if (hash_lookup(table, (HASHKEYTYPE)s[i]) != s[i])
			break;
	printf(fmt, "Lookups:", clock() - start, (max * CLOCKS_PER_SEC)/(clock() - start));

	// Test searching
	if (hash_search(table, Search, "Item 271") != s[271])
		return FALSE;
	if (hash_search(table, Search, "Not Found") != FALSE)
		return FALSE;

	// Test iterating
	ctr = 0;
	start = clock();
	hash_search(table, Iterate, &ctr);
	printf(fmt, "Iterate:", clock() - start, (max * CLOCKS_PER_SEC)/(clock() - start));
	printf("\t\t(Ctr is %u)\n", ctr);

	// Test deleting
	start = clock();
	for (i = 0; i < max; i++)
		if (hash_remove(table, (HASHKEYTYPE)s[i]) != s[i])
			break;
	printf(fmt, "Deletes:", clock() - start, (max * CLOCKS_PER_SEC)/(clock() - start));
	if (hash_count(table) != 0)
		return FALSE;
	table = hash_destroy(table);

	// Free the test strings
	for (i = 0; i < max; i++)
		free(s[i]);


	return TRUE;
}


