/*
   miniunz.c
   Version 1.1, February 14h, 2010
   sample part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications of Unzip for Zip64
         Copyright (C) 2007-2008 Even Rouault

         Modifications for Zip64 support on both zip and unzip
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )
*/

#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
        #ifndef __USE_FILE_OFFSET64
                #define __USE_FILE_OFFSET64
        #endif
        #ifndef __USE_LARGEFILE64
                #define __USE_LARGEFILE64
        #endif
        #ifndef _LARGEFILE64_SOURCE
                #define _LARGEFILE64_SOURCE
        #endif
        #ifndef _FILE_OFFSET_BIT
                #define _FILE_OFFSET_BIT 64
        #endif
#endif

#if defined(__APPLE__) || defined(IOAPI_NO_64) || defined (__ANDROID__)// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <utime.h>
#include <sys/stat.h>
#endif


#include "unzip.h"

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME (256)

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif
/*
  mini unzip, demo of unzip package

  usage :
  Usage : miniunz [-exvlo] file.zip [file_to_extract] [-d extractdir]

  list the file in the zipfile, and print the content of FILE_ID.ZIP or README.TXT
    if it exists
*/


/* change_file_date : change the date/time of a file
    filename : the filename of the file where date/time must be modified
    dosdate : the new date at the MSDos format (4 bytes)
    tmu_date : the SAME new date at the tm_unz format */
void change_file_date(const char *filename,uLong dosdate,tm_unz tmu_date)
{
#ifdef _WIN32
  HANDLE hFile;
  FILETIME ftm,ftLocal,ftCreate,ftLastAcc,ftLastWrite;

  hFile = CreateFileA(filename,GENERIC_READ | GENERIC_WRITE,
                      0,NULL,OPEN_EXISTING,0,NULL);
  GetFileTime(hFile,&ftCreate,&ftLastAcc,&ftLastWrite);
  DosDateTimeToFileTime((WORD)(dosdate>>16),(WORD)dosdate,&ftLocal);
  LocalFileTimeToFileTime(&ftLocal,&ftm);
  SetFileTime(hFile,&ftm,&ftLastAcc,&ftm);
  CloseHandle(hFile);
#else
#ifdef unix || __APPLE__
  struct utimbuf ut;
  struct tm newdate;
  newdate.tm_sec = tmu_date.tm_sec;
  newdate.tm_min=tmu_date.tm_min;
  newdate.tm_hour=tmu_date.tm_hour;
  newdate.tm_mday=tmu_date.tm_mday;
  newdate.tm_mon=tmu_date.tm_mon;
  if (tmu_date.tm_year > 1900)
      newdate.tm_year=tmu_date.tm_year - 1900;
  else
      newdate.tm_year=tmu_date.tm_year ;
  newdate.tm_isdst=-1;

  ut.actime=ut.modtime=mktime(&newdate);
  utime(filename,&ut);
#endif
#endif
}


/* mymkdir and change_file_date are not 100 % portable
   As I don't know well Unix, I wait feedback for the unix portion */

int mymkdir(const char* dirname)
{
    int ret=0;
#ifdef _WIN32
    ret = _mkdir(dirname);
#elif unix
    ret = mkdir (dirname,0775);
#elif __APPLE__
    ret = mkdir (dirname,0775);
#endif
    return ret;
}

int makedir (const char * newdir)
{
  char *buffer ;
  char *p;
  int  len = (int)strlen(newdir);

  if (len <= 0)
    return 0;

  buffer = (char*)malloc(len+1);
  if (buffer==NULL)
  {
      fprintf(stderr,"Error allocating memory\n");
      return UNZ_INTERNALERROR;
  }
  strcpy(buffer,newdir);

  if (buffer[len-1] == '/') {
      buffer[len-1] = '\0';
  }
  if (mymkdir(buffer) == 0)
  {
      free(buffer);
      return 1;
  }

  p = buffer+1;
  while (1)
  {
      char hold;

      while(*p && *p != '\\' && *p != '/')
          p++;
      hold = *p;
      *p = 0;
      if ((mymkdir(buffer) == -1) && (errno == ENOENT))
      {
          fprintf(stderr,"couldn't create directory %s\n",buffer);
          free(buffer);
          return 0;
      }
      if (hold == 0)
          break;
      *p++ = hold;
  }
  free(buffer);
  return 1;
}

int do_extract_currentfile(unzFile uf,const int* popt_extract_without_path,int* popt_overwrite,const char* password)
{
    char filename_inzip[256];
    char* filename_withoutpath;
    char* p;
    int err=UNZ_OK;
    FILE *fout=NULL;
    void* buf;
    uInt size_buf;

    unz_file_info64 file_info;
    err = unzGetCurrentFileInfo64(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

    if (err!=UNZ_OK)
    {
        fprintf(stderr,"error %d with zipfile in unzGetCurrentFileInfo\n",err);
        return err;
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void*)malloc(size_buf);
    if (buf==NULL)
    {
        fprintf(stderr,"Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }

    p = filename_withoutpath = filename_inzip;
    while ((*p) != '\0')
    {
        if (((*p)=='/') || ((*p)=='\\'))
            filename_withoutpath = p+1;
        p++;
    }

    if ((*filename_withoutpath)=='\0')
    {
        if ((*popt_extract_without_path)==0)
        {
            printf("creating directory: %s\n",filename_inzip);
            mymkdir(filename_inzip);
        }
    }
    else
    {
        const char* write_filename;
        int skip=0;

        if ((*popt_extract_without_path)==0)
            write_filename = filename_inzip;
        else
            write_filename = filename_withoutpath;

        err = unzOpenCurrentFilePassword(uf,password);
        if (err!=UNZ_OK)
        {
            fprintf(stderr,"error %d with zipfile in unzOpenCurrentFilePassword\n",err);
        }

        if (((*popt_overwrite)==0) && (err==UNZ_OK))
        {
            FILE* ftestexist;
            ftestexist = FOPEN_FUNC(write_filename,"rb");
            if (ftestexist!=NULL)
            {
                fclose(ftestexist);
                skip = 1;
            }

        }

        if ((skip==0) && (err==UNZ_OK))
        {
            fout=FOPEN_FUNC(write_filename,"wb");
            /* some zipfile don't contain directory alone before file */
            if ((fout==NULL) && ((*popt_extract_without_path)==0) &&
                                (filename_withoutpath!=(char*)filename_inzip))
            {
                char c=*(filename_withoutpath-1);
                *(filename_withoutpath-1)='\0';
                makedir(write_filename);
                *(filename_withoutpath-1)=c;
                fout=FOPEN_FUNC(write_filename,"wb");
            }

            if (fout==NULL)
            {
                fprintf(stderr,"error opening %s\n",write_filename);
            }
        }

        if (fout!=NULL)
        {
            fprintf(stderr," extracting: %s\n",write_filename);

            do
            {
                err = unzReadCurrentFile(uf,buf,size_buf);
                if (err<0)
                {
                    fprintf(stderr,"error %d with zipfile in unzReadCurrentFile\n",err);
                    break;
                }
                if (err>0)
                    if (fwrite(buf,err,1,fout)!=1)
                    {
                        fprintf(stderr,"error in writing extracted file\n");
                        err=UNZ_ERRNO;
                        break;
                    }
            }
            while (err>0);
            if (fout)
                    fclose(fout);

            if (err==0)
                change_file_date(write_filename,file_info.dosDate,
                                 file_info.tmu_date);
        }

        if (err==UNZ_OK)
        {
            err = unzCloseCurrentFile (uf);
            if (err!=UNZ_OK)
            {
                fprintf(stderr,"error %d with zipfile in unzCloseCurrentFile\n",err);
            }
        }
        else
            unzCloseCurrentFile(uf); /* don't lose the error */
    }

    free(buf);
    return err;
}


int do_extract(unzFile uf,int opt_extract_without_path,int opt_overwrite,const char* password)
{
    uLong i;
    unz_global_info64 gi;
    int err=UNZ_OK;

    err = unzGetGlobalInfo64(uf,&gi);
    if (err!=UNZ_OK) {
        fprintf(stderr,"error %d with zipfile in unzGetGlobalInfo \n",err);
        return err;
    }

    for (i=0;i<gi.number_entry;i++)
    {
        err = do_extract_currentfile(uf,&opt_extract_without_path,
                                     &opt_overwrite,
                                     password);
        if (err != UNZ_OK)
            break;

        if ((i+1)<gi.number_entry)
        {
            err = unzGoToNextFile(uf);
            if (err!=UNZ_OK)
            {
                fprintf(stderr,"error %d with zipfile in unzGoToNextFile\n",err);
                break;
            }
        }
    }

    return err;
}

int do_extract_onefile(unzFile uf,const char* filename,int opt_extract_without_path,int opt_overwrite,const char * password)
{
    if (unzLocateFile(uf,filename,CASESENSITIVITY)!=UNZ_OK)
    {
        fprintf(stderr,"file %s not found in the zipfile\n",filename);
        return 2;
    }

    if (do_extract_currentfile(uf,&opt_extract_without_path,
                                      &opt_overwrite,
                                      password) == UNZ_OK)
        return UNZ_OK;
    else
        return 1;
}

int miniunzip(int mode,const char * zipfilename, const char * dirname,
              const char * filename_to_extract, const char * password) {

    char filename_try[MAXFILENAME+16] = "";
    int ret_value=0;
    int opt_do_extract_withoutpath=((mode&1)!=0);
    int opt_overwrite=((mode&2)!=0);
    int opt_extractdir=dirname!=NULL;

    unzFile uf=NULL;

    if (zipfilename!=NULL)
    {

#ifdef USEWIN32IOAPI
        zlib_filefunc64_def ffunc;
#endif

        strncpy(filename_try, zipfilename,MAXFILENAME-1);
        /* strncpy doesnt append the trailing NULL, of the string is too long. */
        filename_try[ MAXFILENAME ] = '\0';

#ifdef USEWIN32IOAPI
        fill_win32_filefunc64A(&ffunc);
        uf = unzOpen2_64(zipfilename,&ffunc);
#else
        uf = unzOpen64(zipfilename);
#endif
        if (uf==NULL)
        {
            strcat(filename_try,".zip");
#            ifdef USEWIN32IOAPI
            uf = unzOpen2_64(filename_try,&ffunc);
#            else
            uf = unzOpen64(filename_try);
#            endif
        }
    }

    if (uf==NULL)
    {
        fprintf(stderr,"Cannot open %s or %s.zip\n",zipfilename,zipfilename);
        return -1;
    }

    fprintf(stderr,"%s opened\n",filename_try);


#ifdef _WIN32
    if (opt_extractdir && _chdir(dirname))
#else
    if (opt_extractdir && chdir(dirname))
#endif
    {
        fprintf(stderr,"Error changing into %s, aborting\n", dirname);
        return -1;
    }

    if (filename_to_extract == NULL)
        ret_value = do_extract(uf, opt_do_extract_withoutpath, opt_overwrite, password);
    else
        ret_value = do_extract_onefile(uf, filename_to_extract, opt_do_extract_withoutpath, opt_overwrite, password);

    unzClose(uf);

    fprintf(stderr,"Done uncompress");

    return ret_value;
}
