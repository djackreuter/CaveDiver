#include <stdio.h>
#include <Windows.h>

typedef struct _CAVE_INFO
{
	PBYTE pStartAddress;
	PBYTE pEndAddress;
	DWORD dwSize;
} CAVE_INFO, * PCAVE_INFO;

int enumerateSection(PIMAGE_SECTION_HEADER pSectionHeader, PBYTE pBuffer)
{
	PBYTE pRawData = pBuffer + pSectionHeader->PointerToRawData;
	DWORD dwSectionSize = pSectionHeader->SizeOfRawData;
	printf("[+] Raw data pointer: 0x%p\n", pRawData);

	PBYTE pStartAddress = NULL;
	PBYTE pEndAddress = NULL;

	DWORD caveSize = 0;
	PBYTE previousAddress = NULL;
	CAVE_INFO caveInfo[100] = {0};
	int caveIndex = 0;

	for (DWORD i = 0; i <= dwSectionSize; i++)
	{
		if (*(BYTE*)(pRawData + i) == 0xCC ||
			*(BYTE*)(pRawData + i) == 0x90 ||
			*(BYTE*)(pRawData + i) == 0x00)
		{
			previousAddress = pRawData + i;

			if (pStartAddress == NULL)
			{
				pStartAddress = pRawData + i;
			}
			else
			{
				caveSize++;
				pEndAddress = pRawData + i;
			}
		}
		else
		{
			if (caveSize >= 50 && caveIndex <= 99)
			{
				CAVE_INFO cave = {
					pStartAddress,
					pEndAddress,
					caveSize
				};
				printf("CAVE Start: %p\n", cave.pStartAddress);
				printf("CAVE End: %p\n", cave.pEndAddress);
				printf("CAVE Size: %d\n", cave.dwSize);
				caveInfo[caveIndex] = cave;
				caveIndex++;
			}

			pStartAddress = NULL;
			pEndAddress = NULL;
			caveSize = 0;
		}
		/*if ((i + 1) % 16 == 0)
		{
			printf("\n");
		}
		*/
	}

	printf("\033[0;32m[+] Found %d cave(s) in the section.\033[0m\n", caveIndex);
	for (int i = 0; i < caveIndex; i++)
	{
		printf("Cave [%d]:\n", i);
		printf("  Start Address: 0x%p\n", caveInfo[i].pStartAddress);
		printf("  End Address: 0x%p\n", caveInfo[i].pEndAddress);
		printf("  Size: %d bytes\n", caveInfo[i].dwSize);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s <input file>\n", argv[0]);
		return 1;
	}

	DWORD dwBinaryType = 100;
	GetBinaryTypeA(argv[1], &dwBinaryType);

	if (dwBinaryType != SCS_32BIT_BINARY)
	{
		printf("[*] Error: The file is not a 32-bit binary.\n");
		return 1;
	}

	PBYTE pBuffer = NULL;

	HANDLE hFile = CreateFileA(argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("[*] Error opening file: %d\n", GetLastError());
		return 1;
	}

	DWORD dwFileSize = GetFileSize(hFile, NULL);

	printf("[+] File size: %d bytes\n", dwFileSize);

	pBuffer = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize);
	
	if (pBuffer == NULL)
	{
		printf("[*] Error allocating memory: %d\n", GetLastError());
		CloseHandle(hFile);
		return 1;
	}

	if (!ReadFile(hFile, pBuffer, dwFileSize, NULL, NULL))
	{
		printf("[*] Error reading bytes into memory: %d\n", GetLastError());
		CloseHandle(hFile);
		return 1;
	}

	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pBuffer;
	PIMAGE_NT_HEADERS pNTHeaders = (PIMAGE_NT_HEADERS)(pBuffer + pDosHeader->e_lfanew);
	ULONG_PTR entryPoint = (ULONG_PTR)pBuffer + pNTHeaders->OptionalHeader.AddressOfEntryPoint;

	printf("[+] Buffer: 0x%p\n", (void*)pBuffer);

	PIMAGE_SECTION_HEADER sectionHeaders = IMAGE_FIRST_SECTION(pNTHeaders);

	for (int i = 0; i < pNTHeaders->FileHeader.NumberOfSections; i++)
	{
		printf("[+] Section %d: %s\n", i + 1, sectionHeaders[i].Name);
		printf("[+] Data Location: 0x%p\n", pBuffer + sectionHeaders[i].PointerToRawData);
		printf("    Virtual Address: 0x%08X\n", sectionHeaders[i].VirtualAddress);
		printf("    Size of Raw Data: 0x%08X\n", sectionHeaders[i].SizeOfRawData);
		printf("    Pointer to Raw Data: 0x%08X\n", sectionHeaders[i].PointerToRawData);
		if (strcmp((char*)sectionHeaders[i].Name, ".text") == 0)
		{
			printf("    [*] Found %s section!\n", sectionHeaders[i].Name);
			printf("    [*] Virtual Address: 0x%08X\n", sectionHeaders[i].VirtualAddress);
			printf("    [*] Size of Raw Data: 0x%08X\n", sectionHeaders[i].SizeOfRawData);
			printf("    [*] Pointer to Raw Data: 0x%08X\n", sectionHeaders[i].PointerToRawData);
			enumerateSection(&sectionHeaders[i], pBuffer);
		}
	}

	HeapFree(GetProcessHeap(), 0, pBuffer);

	return 0;
}
