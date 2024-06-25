#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define access _access
#else // linux
#include <unistd.h>
#endif

// NOTE(vic): Credit to Tsoding
char *ReadEntireFile(const char *FilePath, size_t *Size)
{
    char *Buffer = NULL;
    
    FILE *f = fopen(FilePath, "rb");
    if(f == NULL) {
        goto error;
    }
    
    if(fseek(f, 0, SEEK_END) < 0) {
        goto error;
    }
    
    long m = ftell(f);
    if(m < 0) {
        goto error;
    }
    
    Buffer = malloc(sizeof(char) * m);
    if(Buffer == NULL) {
        goto error;
    }
    
    if(fseek(f, 0, SEEK_SET) < 0) {
        goto error;
    }
    
    size_t n = fread(Buffer, 1, m, f);
    assert(n == (size_t) m);
    
    if(ferror(f)) {
        goto error;
    }
    
    if(Size) {
        *Size = n;
    }
    
    fclose(f);
    
    return Buffer;
    
    error:
    if(f) {
        fclose(f);
    }
    
    if(Buffer) {
        free(Buffer);
    }
    
    return NULL;
}

String_View sv_ReadEntireFile(const char *FilePath)
{
    String_View Result = {0};
    
    FILE *f = fopen(FilePath, "rb");
    if(f == NULL) {
        goto error;
    }
    
    if(fseek(f, 0, SEEK_END) < 0) {
        goto error;
    }
    
    long m = ftell(f);
    if(m < 0) {
        goto error;
    }
    
    Result.data = malloc(sizeof(char) * m);
    if(Result.data == NULL) {
        goto error;
    }
    
    if(fseek(f, 0, SEEK_SET) < 0) {
        goto error;
    }
    
    size_t n = fread((char *)Result.data, 1, m, f);
    assert(n == (size_t) m);
    
    if(ferror(f)) {
        goto error;
    }
    
    Result.count = n;
    
    fclose(f);
    
    return Result;
    
    error:
    if(f) {
        fclose(f);
    }
    
    if(Result.data) {
        free((char *)Result.data);
    }
    
    return SV_NULL;
}
