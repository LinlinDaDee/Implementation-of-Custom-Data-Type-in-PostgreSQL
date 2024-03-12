#include "postgres.h"
#include "fmgr.h"
#include "libpq/pqformat.h"		/* needed for send/recv functions */
#include "access/hash.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "utils/builtins.h"

PG_MODULE_MAGIC;

typedef struct PersonName
{
	int32 length;
	char pname[FLEXIBLE_ARRAY_MEMBER];
} PersonName;

static int check_input(char * str){
	int result = 0, comma_count = 0;
	int len = strlen(str);
	if (len == 0 || len == 1){
		return 0;
	}
	if (!isupper(str[0])){
		return 0;
	}
	for (int i = 0; i < len; i++){
		/* check symbols ('-39) */
		if (!isalpha(str[i]) && str[i] != ',' && !isspace(str[i]) && str[i] != '-' && str[i] != 39){
			return 0;
		}
		/* no consecutive ' ' */
		if ((i+1)<len && str[i]==' ' && str[i+1]==' '){
			return 0;
		}
		/* names begin without an upper-case letter */
		if ((i+1)<len && str[i] == ' ' && !isupper(str[i+1])){
			return 0;
		}
		if (isupper(str[i])) {
			/* names only contain 1 letter */
			if (i>0 && (str[i-1]==' ' || str[i-1]==',')){
				if (((i+1)<len && ((isspace(str[i+1]) || str[i+1]=='.'))) || (i+1) == len){
				return 0;
				}
			}
		}
		if (str[i]==','){
			if (i<2 || (i>0 && str[i-1]==' ')){
				return 0;
			}
			comma_count ++;
		}
	}
	if (comma_count == 1){
		return 1;
	}
	return result;
}

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(pname_in);
Datum 
pname_in(PG_FUNCTION_ARGS)
{
	char *pname = PG_GETARG_CSTRING(0);
	PersonName *result;
	char *given = NULL;
	int length = 0;

	given = strchr(pname, ',');
	if (!check_input(pname)){
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type %s: \"%s\"",
						"PersonName", PG_GETARG_CSTRING(0))));
	}
	/* FamilyName,GivenNames */
	/* FamilyName, GivenNames */
	*given = '\0';
	given++;
	if (*given == ' '){
		given++;
	}
	/* family: pname, given: given */
	
	length = strlen(pname) + strlen(given) + 2;
	result = (PersonName *) palloc(VARHDRSZ + length);
	SET_VARSIZE(result, VARHDRSZ + length);

	/* saved pname: FamilyName\0GivenNames\0 */
	strcpy(result->pname, pname);
	strcpy(result->pname + strlen(pname) + 1, given);
	
	PG_RETURN_POINTER(result);
}


PG_FUNCTION_INFO_V1(pname_out);

Datum
pname_out(PG_FUNCTION_ARGS)
{
	PersonName *personName = (PersonName *) PG_GETARG_POINTER(0);
	char *result;
	char *given = personName->pname + strlen(personName->pname) + 1;

	result = psprintf("%s,%s", personName->pname, given);
	PG_RETURN_CSTRING(result);
}

/*****************************************************************************
 * Operator class for defining B-tree index
 *
 * It's essential that the comparison operators and support function for a
 * B-tree index opclass always agree on the relative ordering of any two
 * data values.  Experience has shown that it's depressingly easy to write
 * unintentionally inconsistent functions.  One way to reduce the odds of
 * making a mistake is to make all the functions simple wrappers around
 * an internal three-way-comparison function, as we do here.
 *****************************************************************************/

#define Mag(c)	((c)->x*(c)->x + (c)->y*(c)->y)

static int
pname_cmp_internal(PersonName * a, PersonName * b)
{
	int result = 0;
	char *given_a = a->pname + strlen(a->pname) + 1;
	char *given_b = b->pname + strlen(b->pname) + 1;
	/* compare given name first */
	result = strcmp(a->pname, b->pname);
	if (result == 0) {
		result = strcmp(given_a, given_b);
	}
	return result;
}


PG_FUNCTION_INFO_V1(pname_lt);
Datum
pname_lt(PG_FUNCTION_ARGS)
{
	PersonName *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_cmp_internal(a, b) < 0);
}

PG_FUNCTION_INFO_V1(pname_le);
Datum
pname_le(PG_FUNCTION_ARGS)
{
	PersonName *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_cmp_internal(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(pname_eq);
Datum
pname_eq(PG_FUNCTION_ARGS)
{
	PersonName *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_cmp_internal(a, b) == 0);
}

PG_FUNCTION_INFO_V1(pname_n_eq);
Datum
pname_n_eq(PG_FUNCTION_ARGS)
{
	PersonName *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_cmp_internal(a, b) != 0);
}

PG_FUNCTION_INFO_V1(pname_ge);
Datum
pname_ge(PG_FUNCTION_ARGS)
{
	PersonName *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_cmp_internal(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(pname_gt);
Datum
pname_gt(PG_FUNCTION_ARGS)
{
	PersonName *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_cmp_internal(a, b) > 0);
}

PG_FUNCTION_INFO_V1(pname_cmp);
Datum
pname_cmp(PG_FUNCTION_ARGS)
{
	PersonName *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(pname_cmp_internal(a, b));
}

/* 3 new functions */
PG_FUNCTION_INFO_V1(family);
Datum
family(PG_FUNCTION_ARGS)
{ 
	PersonName *personName = (PersonName *) PG_GETARG_POINTER(0);
	char *result;

	result = psprintf("%s", personName->pname);
	PG_RETURN_TEXT_P(cstring_to_text(result));
}

PG_FUNCTION_INFO_V1(given);
Datum
given(PG_FUNCTION_ARGS)
{
	char *result;
	PersonName *personName = (PersonName *) PG_GETARG_POINTER(0);
	char *given = personName->pname + strlen(personName->pname) + 1;

	result = psprintf("%s", given);
	PG_RETURN_TEXT_P(cstring_to_text(result));
}

PG_FUNCTION_INFO_V1(show);
Datum
show(PG_FUNCTION_ARGS)
{
	PersonName *personName = (PersonName *) PG_GETARG_POINTER(0);
	char *result;
	char *given = personName->pname + strlen(personName->pname) + 1;
	char *index = strchr(given, ' ');
	if (index != NULL){
		*index = '\0';
	}

	result = psprintf("%s %s", given, personName->pname);
	PG_RETURN_TEXT_P(cstring_to_text(result));
}

PG_FUNCTION_INFO_V1(pname_hash);
Datum
pname_hash(PG_FUNCTION_ARGS)
{
	PersonName *personName = (PersonName *) PG_GETARG_POINTER(0);
	char *result = NULL;
	int hash_code = 0;
	/* same hash */
	/* FamilyName,GivenNames */
	/* FamilyName, GivenNames */
	char *given = personName->pname + strlen(personName->pname)	+ 1;
	result = psprintf("%s,%s", personName->pname, given);
	hash_code = DatumGetUInt32(hash_any((const unsigned char *) result, strlen(result)));
	PG_RETURN_INT32(hash_code);
}
