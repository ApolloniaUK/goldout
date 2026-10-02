
/*
 Copyright (c) 2017, Stephane Sudre
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
 - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 - Neither the name of the WhiteBox nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
            File Name: main.c
              Project: goldout
      Original Author: S.Sudre
    Last modification: 10/01/26
    
    +----------+-------------+---------------------------------------------------------+
    |   Date   |    Author   | Comments                                                |
    +----------+-------------+---------------------------------------------------------+
    | 10/01/26 | A.Staniforth| Version 1.1                                             |
    |          |             |                                                         |
    |          |             | - Add -n/--nolegacy                                     |
    |          |             | - Add --norecurse                                       |
    |          |             | - Make -h exit with status 0                            |
    |          |             | - Fix processing of relative paths                      |
    |          |             | - Skip writing empty resource forks                     |
    |          |             | - Fix unneeded processing of FinderInfo only            |
    |          |             |   AppleDouble files when --nosetinfo is passed          |
    |          |             | - Fix the AppleDouble header check on big-endian Macs   |
    |          |             | - Fix memory leaks                                      |
    |          |             | - Reject a FinderInfo entry shorter than 32 bytes       |
    |          |             | - Replace an existing resource fork instead of writing  |
    |          |             |   over the start of it                                  |
    |          |             | - Check the resource fork was written completely and    |
    |          |             |   keep the AppleDouble file if not                      |
    |          |             | - Exit with status 1 if any file could not be processed |
    |          |             | - Report errors that previously failed silently         |
    |          |             | - Make error messages clearer and more consistent       |
    +----------+-------------+---------------------------------------------------------+
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <fts.h>

#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include <CoreFoundation/CoreFoundation.h>

#define GOLDOUT_VERSION "1.1"

boolean_t gQuiet=false;
boolean_t gNoDelete=false;
boolean_t gSetNoInfo=false;
boolean_t gNoRecurse=false;
boolean_t gNoLegacy=false;
boolean_t gHadError=false;

enum
{
	ASF_RESOURCEFORK=2,
	ASF_FINDERINFO=9
};

typedef struct asf_entry_t
{
    uint32_t entryID;
    uint32_t entryOffset;
    uint32_t entryLength;
} asf_entry_t;

#define FINDER_INFO_SIZE 32

static void usage(const char * inProcesspath)
{
	printf("usage: %s [options] [PATH ...]\n",inProcesspath);
	printf("\n"\
		   "Options:\n"\
	       "  --help, -h           Show this usage guide\n"\
	       "  --quiet, -q          Do not display the list of processed files\n"\
	       "  --nolegacy, -n       Require a path instead of defaulting to /\n"\
	       "  --nosetinfo          Do not set the FinderInfo\n"\
	       "  --nodelete           Do not delete the AppleDouble file\n"\
	       "  --norecurse          Do not descend into subdirectories\n"\
	       "  --version            Show the version of this tool\n"\
		   "\n");
}

// Report an error and remember it, so that goldout exits with a failure status at the end

static void reportError(const char * inFormat, ...)
{
    va_list tArguments;
    
    gHadError=true;
    
    va_start(tArguments, inFormat);
    vfprintf(stderr, inFormat, tArguments);
    va_end(tArguments);
}

// Remove a data file's resource fork, so that a failed or partial write leaves nothing behind

static void removeResourceFork(int inFileDescriptor)
{
    fremovexattr(inFileDescriptor, XATTR_RESOURCEFORK_NAME, 0);
}

int fixUpResourceFork(char * inADFFileName,char * inADFPath)
{
    if (inADFFileName==NULL || inADFPath==NULL)
        return 0;
    
    if (gQuiet==false)
        puts(inADFPath);
    
    
    int tFileDescriptor=open(inADFPath,O_RDONLY);
    
    if (tFileDescriptor<0)
    {
        reportError("Error processing \"%s\" (%s)\n", inADFPath, strerror(errno));
        
        return 0;
    }
    
    // Read the first 26 bytes to check whether it's an AppleDouble file
	//  4 bytes: Magic Number
	//  4 bytes: Version Number
	// 16 bytes: Filler
	//  2 bytes: Number of entries
    
    uint8_t tBuffer[26];
    
    uint32_t tMagicNumber;
    uint32_t tVersionNumber;
    
    ssize_t tReadSize=read(tFileDescriptor,tBuffer,sizeof(tBuffer));
    
    if (tReadSize<0)
    {
        close(tFileDescriptor);
        
		switch (errno)
		{
			case EIO:
				
				reportError("Error processing \"%s\" (An I/O error occurred while reading from the file system)\n", inADFPath);
				
				break;
				
			case ENOMEM:
				
				reportError("Error processing \"%s\" (Memory is low)\n", inADFPath);
				
				break;
				
			default:
				
				reportError("Error processing \"%s\" (%s)\n", inADFPath, strerror(errno));
				
				break;
		}
		
        return 0;
    }
    
    if (tReadSize!=sizeof(tBuffer))
    {
        close(tFileDescriptor);
        
        reportError("Error processing \"%s\" (Not an AppleDouble file)\n", inADFPath);
        
        return 0;
    }
    
    memcpy(&tMagicNumber, tBuffer, sizeof(uint32_t));

    tMagicNumber=CFSwapInt32BigToHost(tMagicNumber);

#define APPLE_DOUBLE_MAGIC_NUMBER   0x00051607

    
    if (tMagicNumber!=APPLE_DOUBLE_MAGIC_NUMBER)
    {
        close(tFileDescriptor);
		
		reportError("Error processing \"%s\" (Not an AppleDouble file)\n", inADFPath);
		
        return 0;
    }
    
    memcpy(&tVersionNumber, tBuffer+4, sizeof(uint32_t));

    tVersionNumber=CFSwapInt32BigToHost(tVersionNumber);

#define APPLE_DOUBLE_VERSION        0x00020000
    
    if (tVersionNumber!=APPLE_DOUBLE_VERSION)
    {
        close(tFileDescriptor);
		
		reportError("Error processing \"%s\" (Unsupported version of AppleDouble)\n", inADFPath);
		
        return 0;
    }
    
    // Number of Entries
    
    uint16_t tNumberOfEntries;
    
    memcpy(&tNumberOfEntries, tBuffer+24, sizeof(uint16_t));
    
#ifdef __LITTLE_ENDIAN__
    
    tNumberOfEntries=CFSwapInt16(tNumberOfEntries);
    
#endif
    
    if (tNumberOfEntries==0)
    {
        close(tFileDescriptor);
        
        if (gNoDelete==false)
        {
            if (unlink(inADFPath)!=0)
            {
                reportError("Error processing \"%s\" (%s)\n", inADFPath, strerror(errno));
            }
        }
        
        return 0;
    }
    
    // Compute the "data fork" path
    
    char tDataPath[PATH_MAX];
    
    strncpy(tDataPath,inADFPath, PATH_MAX);
    
    size_t tLength=strlen(inADFPath);
    size_t tFileNameLength=strlen(inADFFileName)-2;
    
    memmove(tDataPath+tLength-tFileNameLength-2, tDataPath+tLength-tFileNameLength, tFileNameLength+1);
    
    // Iterate through the Entries and restore what we can
    
    asf_entry_t * tEntriesArray=calloc(sizeof(asf_entry_t), tNumberOfEntries);
    
    if (tEntriesArray==NULL)
    {
        close(tFileDescriptor);
		
		reportError("Error processing \"%s\" (Memory is low)\n", inADFPath);
        
        return -1;
    }
    
    uint16_t tEntryIndex=0;
    
    boolean_t tNeedsToFixUp=false;
    
    while (tEntryIndex<tNumberOfEntries)
    {
        uint8_t tEntryBuffer[12];
        
        ssize_t tReadSize=read(tFileDescriptor,tEntryBuffer,sizeof(tEntryBuffer));
        
        if (tReadSize<0)
        {
            close(tFileDescriptor);

			switch (errno)
			{
				case EIO:

					reportError("Error processing \"%s\" (An I/O error occurred while reading from the file system)\n", inADFPath);

					break;

				case ENOMEM:

					reportError("Error processing \"%s\" (Memory is low)\n", inADFPath);

					break;

				default:

					reportError("Error processing \"%s\" (%s)\n", inADFPath, strerror(errno));

					break;
			}

            free(tEntriesArray);

            return 0;
        }

        if (tReadSize!=12)
        {
            close(tFileDescriptor);

            free(tEntriesArray);

            reportError("Error processing \"%s\" (Missing or incomplete entry descriptor in AppleDouble file)\n", inADFPath);

            return 0;
        }
        
        uint32_t tEntryID;
        
        memcpy(&tEntryID, tEntryBuffer, sizeof(uint32_t));
        
#ifdef __LITTLE_ENDIAN__
        
        tEntryID=CFSwapInt32(tEntryID);
        
#endif
        
        tEntriesArray[tEntryIndex].entryID=tEntryID;
        
        if (tEntryID!=ASF_RESOURCEFORK && !(tEntryID==ASF_FINDERINFO && gSetNoInfo==false))
        {
            // We don't deal with other types of entries
            
            tEntryIndex++;
            
            continue;
        }
        
        tNeedsToFixUp=true;
        
        uint32_t tEntryOffset;
        uint32_t tEntryLength;
        
        
        memcpy(&tEntryOffset, tEntryBuffer+sizeof(uint32_t), sizeof(uint32_t));
        
#ifdef __LITTLE_ENDIAN__
        
        tEntryOffset=CFSwapInt32(tEntryOffset);
        
#endif
        
        tEntriesArray[tEntryIndex].entryOffset=tEntryOffset;
        
        memcpy(&tEntryLength, tEntryBuffer+2*sizeof(uint32_t), sizeof(uint32_t));
        
#ifdef __LITTLE_ENDIAN__
        
        tEntryLength=CFSwapInt32(tEntryLength);
        
#endif
        
        tEntriesArray[tEntryIndex].entryLength=tEntryLength;
        
        if (tEntryID==ASF_FINDERINFO && tEntryLength>0 && tEntryLength<FINDER_INFO_SIZE)
        {
            close(tFileDescriptor);
            
            free(tEntriesArray);
            
            reportError("Error processing \"%s\" (The FinderInfo entry is too short)\n", inADFPath);
            
            return 0;
        }
        
        tEntryIndex++;
        
    }
    
    if (tNeedsToFixUp==false)
    {
        close(tFileDescriptor);

        free(tEntriesArray);

        if (gNoDelete==false)
        {
            if (unlink(inADFPath)!=0)
            {
				switch(errno)
				{
					case EROFS:
						
						reportError("Error processing \"%s\" (File cannot be deleted - Read-only file system)\n", inADFPath);
						
						break;
						
					default:
						
						reportError("Error processing \"%s\" (File cannot be deleted (%s))\n", inADFPath,strerror(errno));
						
						break;
				}
            }
        }
        
        return 0;
    }
    
    int tDataFileDescriptor=open(tDataPath,O_RDONLY);
	
    if (tDataFileDescriptor<0)
    {
		switch(errno)
		{
			case ENOENT:
				
				reportError("Error processing \"%s\" (The data file was not found)\n", inADFPath);
				
				break;
			
			case EACCES:
				
				reportError("Error processing \"%s\" (Not enough permission to open the file)\n", inADFPath);

				break;

			default:
				
				reportError("Error processing \"%s\" (Unable to open the data file (%s))\n", inADFPath,strerror(errno));
				
				break;
		}
		
		close(tFileDescriptor);

        free(tEntriesArray);

        return 0;
    }

    // Folders cannot have a resource fork (goldin still writes an empty resource fork entry for them)

    struct stat tDataStat;
    boolean_t tDataIsDirectory=false;

    if (fstat(tDataFileDescriptor,&tDataStat)==0 && S_ISDIR(tDataStat.st_mode))
        tDataIsDirectory=true;

    tEntryIndex=0;
    
#define EXTENDED_ATTRIBUTES_BUFFER_SIZE (256*1024)
    
    uint8_t tExtendedAttributesBuffer[EXTENDED_ATTRIBUTES_BUFFER_SIZE];
    
    while (tEntryIndex<tNumberOfEntries)
    {
        asf_entry_t tEntry=tEntriesArray[tEntryIndex];
        
        switch(tEntry.entryID)
        {
            case ASF_RESOURCEFORK:
            {
                if (tEntry.entryLength==0 || tDataIsDirectory==true)
                    break;

                uint32_t tRemainingData=tEntry.entryLength;
                uint32_t tAlreadyReadSize=0;

                // Writing at position 0 does not shorten an existing, longer resource fork,
                // so remove any existing one first

                if (fremovexattr(tDataFileDescriptor, XATTR_RESOURCEFORK_NAME, 0)!=0 && errno!=ENOATTR)
                {
                    reportError("Error processing \"%s\" (Unable to replace the existing resource fork of the data file (%s))\n", inADFPath, strerror(errno));

                    close(tDataFileDescriptor);

                    close(tFileDescriptor);

                    free(tEntriesArray);

                    return 0;
                }

                lseek(tFileDescriptor, tEntry.entryOffset, SEEK_SET);
                
                do
                {
                    uint32_t tToReadSize=EXTENDED_ATTRIBUTES_BUFFER_SIZE;
                    
                    if (tToReadSize>tRemainingData)
                        tToReadSize=tRemainingData;
                    
                    ssize_t tReadSize=read(tFileDescriptor,tExtendedAttributesBuffer,tToReadSize);
                    
                    if (tReadSize<0)
                    {
                        int tReadErrno=errno;

                        removeResourceFork(tDataFileDescriptor);

						close(tDataFileDescriptor);
                        
                        close(tFileDescriptor);
                        
						switch (tReadErrno)
						{
							case EIO:
								
								reportError("Error processing \"%s\" (An I/O error occurred while reading from the file system)\n", inADFPath);
								
								break;
								
							case ENOMEM:
								
								reportError("Error processing \"%s\" (Memory is low)\n", inADFPath);
								
								break;
								
							default:
								
								reportError("Error processing \"%s\" (%s)\n", inADFPath, strerror(tReadErrno));

								break;
						}

						free(tEntriesArray);

						return 0;
                    }

                    if (tReadSize!=tToReadSize)
                    {
                        reportError("Error processing \"%s\" (The AppleDouble file total size is smaller than what the header states)\n", inADFPath);

                        removeResourceFork(tDataFileDescriptor);

                        close(tDataFileDescriptor);

                        close(tFileDescriptor);

                        free(tEntriesArray);

                        return 0;
                    }

                    if (fsetxattr(tDataFileDescriptor, XATTR_RESOURCEFORK_NAME, tExtendedAttributesBuffer, tReadSize, tAlreadyReadSize, 0)!=0)
                    {
                        reportError("Error processing \"%s\" (Unable to write the resource fork of the data file (%s))\n", inADFPath, strerror(errno));

                        removeResourceFork(tDataFileDescriptor);

                        close(tDataFileDescriptor);

                        close(tFileDescriptor);

                        free(tEntriesArray);

                        return 0;
                    }
                    
                    tAlreadyReadSize+=tReadSize;
                    tRemainingData-=tReadSize;
                }
                while (tRemainingData>0);

                // Check the whole fork was written: when an HFS+ disk is full, fsetxattr()
                // reports success but writes nothing

                if (fgetxattr(tDataFileDescriptor, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, 0)!=(ssize_t) tEntry.entryLength)
                {
                    reportError("Error processing \"%s\" (The resource fork of the data file could not be written completely; the disk may be full)\n", inADFPath);

                    removeResourceFork(tDataFileDescriptor);

                    close(tDataFileDescriptor);

                    close(tFileDescriptor);

                    free(tEntriesArray);

                    return 0;
                }
            }
                break;
                
            case ASF_FINDERINFO:
                
                if (gSetNoInfo==false)
                {
                    lseek(tFileDescriptor, tEntry.entryOffset, SEEK_SET);
                    
					if (tEntry.entryLength==0)
						break;
					
					ssize_t tReadSize=FINDER_INFO_SIZE;
					
					tReadSize=read(tFileDescriptor,tExtendedAttributesBuffer,tReadSize);
                    
                    if (tReadSize<0)
                    {
						switch (errno)
						{
							case EIO:
								
								reportError("Error processing \"%s\" (An I/O error occurred while reading from the file system)\n", inADFPath);
								
								break;
								
							case ENOMEM:
								
								reportError("Error processing \"%s\" (Memory is low)\n", inADFPath);
								
								break;
								
							default:
								
								reportError("Error processing \"%s\" (%s)\n", inADFPath, strerror(errno));
								
								break;
						}
                        
                        close(tDataFileDescriptor);

                        close(tFileDescriptor);

                        free(tEntriesArray);

                        return 0;
                    }

                    if (tReadSize!=FINDER_INFO_SIZE)
                    {
                        reportError("Error processing \"%s\" (The AppleDouble file total size is smaller than what the header states)\n", inADFPath);

                        close(tDataFileDescriptor);

                        close(tFileDescriptor);

                        free(tEntriesArray);

                        return 0;
                    }

                    if (fsetxattr(tDataFileDescriptor, XATTR_FINDERINFO_NAME, tExtendedAttributesBuffer, FINDER_INFO_SIZE, 0, 0)!=0)
                    {
                        reportError("Error processing \"%s\" (Unable to write the Finder information of the data file (%s))\n", inADFPath, strerror(errno));

                        close(tDataFileDescriptor);

                        close(tFileDescriptor);

                        free(tEntriesArray);

                        return 0;
                    }
					
					if (tEntry.entryLength>FINDER_INFO_SIZE)
					{
						// There are probably additional extended attributes to restore
						
						// A COMPLETER
					}
                }
				
                break;
        }
		
        tEntryIndex++;
        
    }
    
    close(tDataFileDescriptor);

    close(tFileDescriptor);

    free(tEntriesArray);

    if (gNoDelete==false)
    {
        if (unlink(inADFPath)!=0)
        {
			switch(errno)
			{
				case EROFS:
					
					reportError("Error processing \"%s\" (File cannot be deleted - Read-only file system)\n", inADFPath);
					
					break;
					
				default:
					
					reportError("Error processing \"%s\" (File cannot be deleted (%s))\n", inADFPath,strerror(errno));
					
					break;
			}
        }
    }
	
    return 0;
}

int fixUpResourceForks(char * const inPath)
{
    char tResolvedPath[PATH_MAX+1];
    
    char * tRealPath=realpath(inPath, (char *)&tResolvedPath);
    
    if (tRealPath==NULL)
    {
        reportError("Error processing \"%s\" (%s)\n", inPath, strerror(errno));

        return -1;
    }

    struct stat tStat;

    if (lstat(tRealPath,&tStat)!=0)
    {
        reportError("Error processing \"%s\" (%s)\n", inPath, strerror(errno));

        return -1;
    }

    struct statfs tStatfs;

    if ((statfs(tResolvedPath, &tStatfs) != 0))
    {
        reportError("Error processing \"%s\" (Unable to get file system information (%s))\n", inPath, strerror(errno));

        return -1;
    }
    
    
    if (strcmp(tStatfs.f_fstypename, "hfs")  != 0 &&
        strcmp(tStatfs.f_fstypename, "apfs") != 0)
    {
        reportError("Error processing \"%s\" (Not on an HFS, HFS+ or APFS volume)\n", inPath);
        
        return -1;
    }
    
    char * const tDirectories[]={inPath,NULL};
    
    FTS* tFileSystem = fts_open(tDirectories,FTS_XDEV|FTS_PHYSICAL|FTS_NOSTAT|FTS_NOCHDIR,NULL);
    
    if (tFileSystem==NULL)
    {
        reportError("Error processing \"%s\" (Unable to traverse the directory (%s))\n", inPath, strerror(errno));

        return -1;
    }
    
    FTSENT* tParent;
    
    while ((tParent=fts_read(tFileSystem)))
    {
        switch(tParent->fts_info)
        {
            case FTS_D:

                // Only examine the contents of the directory passed as argument

                if (gNoRecurse==true && tParent->fts_level>FTS_ROOTLEVEL)
                    fts_set(tFileSystem, tParent, FTS_SKIP);

                break;

			case FTS_DP:

                break;
                
            case FTS_DNR:
            case FTS_ERR:
                
                fprintf(stderr, "Error processing \"%s\" (%s)\n", tParent->fts_path, strerror(tParent->fts_errno));

                break;

            default:

                if (strncmp(tParent->fts_name,"._",2)==0)
                {
                    if (fixUpResourceFork(tParent->fts_name,tParent->fts_path)!=0)
                    {
                        fts_close(tFileSystem);

                        return -1;
                    }
                }
				
                break;
        }
    }
    
    fts_close(tFileSystem);
    
    return 0;
}

int main(int argc, const char * argv[])
{
    
    int c;
    
    while (1)
    {
        static struct option tLongOptions[] =
        {
            {"help",						no_argument,		NULL,	'h'},
			{"quiet",						no_argument,		NULL,	'q'},
			{"nolegacy",					no_argument,		NULL,	'n'},
            {"nosetinfo",					no_argument,		NULL,	0},
            {"nodelete",					no_argument,		NULL,	0},
            {"norecurse",					no_argument,		NULL,	0},
            {"version",                     no_argument,        NULL,	0},

            {0, 0, 0, 0}
        };

        int tOptionIndex = 0;

        c = getopt_long (argc, (char **) argv, "hnq",tLongOptions, &tOptionIndex);
        
        /* Detect the end of the options. */
        if (c == -1)
            break;
        
        switch (c)
        {
            case 0:
				
                if (strncmp(tLongOptions[tOptionIndex].name,"version",strlen("version"))==0)
                {
					printf("%s\n",GOLDOUT_VERSION);
					
					exit(0);
                }
                else if (strncmp(tLongOptions[tOptionIndex].name,"nosetinfo",strlen("nosetinfo"))==0)
                {
                    gSetNoInfo=true;
                }
                else if (strncmp(tLongOptions[tOptionIndex].name,"nodelete",strlen("nodelete"))==0)
                {
                    gNoDelete=true;
                }
                else if (strncmp(tLongOptions[tOptionIndex].name,"norecurse",strlen("norecurse"))==0)
                {
                    gNoRecurse=true;
                }

                break;

            case 'q':

                gQuiet=true;

                break;

            case 'n':

                gNoLegacy=true;

                break;
                
            case 'h':

				usage(argv[0]);

				exit(0);

			default:

				usage(argv[0]);

				exit(2);

				break;
        }
    }
    
    const char * tProcessPath=argv[0];

    argv+=optind;
    argc-=optind;

    const char * tDefaultListOfFiles[]={"/"};

    const char ** tArguments;

    if (argc < 1)
    {
        // Like FixupResourceForks, default to the whole boot volume unless --nolegacy was given

        if (gNoLegacy==true)
        {
            fprintf(stderr, "No path was specified\n");

            usage(tProcessPath);

            exit(2);
        }

        argc=1;
        
        tArguments=tDefaultListOfFiles;
    }
    else
    {
        tArguments=argv;
    }
    
    int tIndex=0;
    
    do
    {
        int tStatus=fixUpResourceForks((char * const)tArguments[tIndex]);
        
        if (tStatus!=0)
            exit(EXIT_FAILURE);
        
        tIndex++;
    }
    while (tIndex<argc);
    
    // Some AppleDouble files could not be processed
    
    if (gHadError==true)
        return EXIT_FAILURE;
    
    return 0;
}
