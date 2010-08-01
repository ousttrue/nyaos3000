#include <string.h>
#ifdef __EMX__
#  define INCL_DOSNLS
#  include <os2.h>
#else
#  include <windows.h>
#endif

char dbcs_table[ 256 + 128 ];
#define isKanji(n) (dbcs_table+128)[n]

void init_dbcs_table()
{
#ifdef __EMX__
    char buffer[12];

    COUNTRYCODE country;

    country.country = 0;
    country.codepage = 0;

    int rc=(int)DosQueryDBCSEnv((ULONG)numof(buffer)
				,&country
				,buffer);
    char *p=buffer;
    while( (p[0]!=0 || p[1] !=0) && p < buffer+sizeof(buffer) ){
      /* printf("DBCS %02x--%02x\n",p[0] & 255 ,p[1] & 255); */
      memset( (dbcs_table+128)+(p[0] & 255), 1
	     , (p[1] & 255)-(p[0] & 255)+1 );
      p += 2;
    }
#else
    for(int i=0;i<256;i++){
        isKanji( i ) = IsDBCSLeadByte( i );
    }
#endif
    memcpy( dbcs_table , dbcs_table+256 , 128 );
}
