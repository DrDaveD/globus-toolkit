/*
 * Portions of this file Copyright 1999-2005 University of Chicago
 * Portions of this file Copyright 1999-2005 The University of Southern California.
 *
 * This file or a portion of this file is licensed under the
 * terms of the Globus Toolkit Public License, found at
 * http://www.globus.org/toolkit/download/license.html.
 * If you redistribute this file, with or without
 * modifications, you must include this notice in the file.
 */

#include "globus_common.h"

int
main(int argc, char *argv[])
{
    FILE *              fptr, *outptr;
    char                line[1024];
    char		keyline[1024];
    char *		newlineptr;
    char *		it;
    char * 		out;
    int			hash;

    if (argc<2)
    {
	printf("Usage:  globus-i18n-resource-init <infile> <outfile>\n");
	return GLOBUS_FAILURE;
    }
    
    fptr = fopen(argv[1], "r");
    if (fptr==NULL)
    {
	printf("File %s could not be opened for reading\n", argv[1]);
	return GLOBUS_FAILURE;
    }
    outptr = fopen(argv[2], "wt");
    if (outptr==NULL)
    {
	printf("File %s could not be opened for writing\n", argv[2]);
	return GLOBUS_FAILURE;
    }


    fprintf(outptr, "root { \n\n");


    while(fgets(line, sizeof(line), fptr) != NULL)
    {
   /* strncpy(keyline,line, 1024);*/
    /*sprintf(&keyline, "%s", line);
    fprintf(outptr, "%s", line);*/

    /*convert non-invariant characters to "_" for key*/
    it=line; 
    out=keyline;

    /*get rid of trailing \n*/
    while (it[0]!='\n')
    {
	it++;
    }
    it[0]='\0';
    it=line;
    /*collapse \n to return line char*/
    while (it[0]!='\0')
    {
	switch (it[0])
	{
                case '\\':

		    if (it[1]=='n')
		    {
		        out[0]='\n';
		        it++;
		    }
		    else
		    {
			out[0]=it[0];
		    }
		    break;
                default:
		    out[0]=it[0];
                        /*we don't need to do anything*/
                        break;
        }
	it++;
	out++;
    }
    out[0]='\0';  /* need the NULL termination*/
	    
	hash=globus_hashtable_string_hash(keyline, 35535);
	it=keyline;
    while (it[0]!='\0')
    {   
        switch (it[0])
        {
                case '#':
                case '!':
                case '@':
                case '[':
                case ']':
                case '^':
                case '`':
                case '{':
                case '|':
                case '}':
                case '~':
                case ' ':
		case '\n':


                it[0]= '_';

                   break;

                default:
                        /*we don't need to do anything*/
                        break;
        }
            it++;
    }

	newlineptr = strchr(line, '\n');
	if (newlineptr!=NULL)
	{
		*newlineptr=NULL;
	}

	newlineptr = strchr(keyline, '\n');
	if (newlineptr!=NULL)
	{
		*newlineptr=NULL;
	}

	fprintf(outptr, "\"%s_%d\"     {\"%s\"}\n", keyline, hash, line);
    }
    fprintf(outptr, "}");

    return GLOBUS_SUCCESS;
}
