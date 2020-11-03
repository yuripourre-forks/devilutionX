/**
 * @file encrypt.h
 *
 * Interface of functions for compression and decompressing MPQ data.
 */
#ifndef __ENCRYPT_H__
#define __ENCRYPT_H__

DEVILUTION_BEGIN_NAMESPACE

#ifdef __cplusplus
extern "C" {
#endif

void Decrypt(DWORD *castBlock, DWORD size, DWORD key);
void Encrypt(DWORD *castBlock, DWORD size, DWORD key);
DWORD Hash(const char *s, int type);
void InitHash();
int PkwareCompress(BYTE *srcData, int size);
void PkwareDecompress(BYTE *pbInBuff, int recv_size, int dwMaxBytes);

#ifdef __cplusplus
}
#endif

DEVILUTION_END_NAMESPACE

#endif /* __ENCRYPT_H__ */
