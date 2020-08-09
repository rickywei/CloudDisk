#ifndef BASE64_H
#define BASE64_H

char* base64_encode(const unsigned char* bindata, int binlength, char* base64);

int base64_decode(const char* base64, unsigned char* bindata);

#endif
