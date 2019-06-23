#include <Foundation/Foundation.h>
#include <string.h>

int GetTempPath(size_t buflen, char *buffer)
{ @autoreleasepool
{
    NSString *str = NSTemporaryDirectory () ;
    const char *temppath = [str fileSystemRepresentation] ;
    if (temppath == NULL) return 0 ;
    const size_t tmplen = strlen (temppath) + 1 ;
    if (tmplen > buflen) return tmplen ;
    strcpy (buffer, temppath) ;
    return tmplen - 1 ;
}}
