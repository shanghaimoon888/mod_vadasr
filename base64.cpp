

#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <string.h>

void base64_encode_block(char *inData, int inlen, char *outData, int *outlen)
{
	if (NULL == inData)
	{
		return;
	}

	int blocksize;
	blocksize = inlen * 8 / 6 + 3;
	unsigned char* buffer = new unsigned char[blocksize];
	memset(buffer, 0, blocksize);
	*outlen = EVP_EncodeBlock(buffer, (const unsigned char*)inData, inlen);
	strcpy(outData, (char*)buffer);

	delete buffer;

}
