#include "Core.h"
#include "rijndael.h"

// AES-256 decrypt in-place (ECB), used for encrypted UE3 package headers.
void appDecryptAES(byte* Data, int Size, const char* Key, int KeyLen)
{
	guard(appDecryptAES);

	if (KeyLen < 0)
		KeyLen = (int)strlen(Key);
	if (KeyLen <= 0 || Size <= 0)
		return;

	// Pad key to 32 bytes for AES-256
	unsigned char keybuf[32];
	memset(keybuf, 0, sizeof(keybuf));
	memcpy(keybuf, Key, KeyLen > 32 ? 32 : KeyLen);

	unsigned long rk[RKLENGTH(256)];
	int nrounds = rijndaelSetupDecrypt(rk, keybuf, 256);

	// Process full 16-byte blocks
	int blocks = Size / 16;
	for (int i = 0; i < blocks; i++)
	{
		unsigned char* block = Data + i * 16;
		unsigned char plain[16];
		rijndaelDecrypt(rk, nrounds, block, plain);
		memcpy(block, plain, 16);
	}

	unguard;
}
