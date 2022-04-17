// Copyright (c) 2021 onein528
// Licensed under the MIT License.

#include <stdio.h>
#include <windows.h>

#define CHECKRETURN(x) ((x) == 0)

BOOL CALLBACK ExportMessageTable(HINSTANCE hModule, LPCWSTR lpszType, LPCWSTR lpszName, WORD wIDLanguage, LONG_PTR lParam);


int wmain(int argc, LPWSTR argv[]) {

    BOOL bResult = FALSE;
    HMODULE hModule = NULL;
    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (argc != 3) return 1;

    hModule = LoadLibraryExW(argv[1], NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
    if (!hModule) goto CleanUp;

    hFile = CreateFileW(argv[2], GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) goto CleanUp;

    bResult = EnumResourceLanguagesExW(hModule, RT_MESSAGETABLE, MAKEINTRESOURCEW(1), ExportMessageTable, (LONG_PTR)hFile, RESOURCE_ENUM_LN | RESOURCE_ENUM_MUI, (LANGID)0);

CleanUp:
    if (hModule) FreeLibrary(hModule);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

    exit(!bResult || GetLastError());
}


BOOL CALLBACK ExportMessageTable(HINSTANCE hModule, LPCWSTR lpszType, LPCWSTR lpszName, WORD wIDLanguage, LONG_PTR lParam) {

    HRSRC hRes = FindResourceExW((HMODULE)hModule, lpszType, lpszName, wIDLanguage);
    if (!hRes) return FALSE;
    HGLOBAL hGlobal = LoadResource(hModule, hRes);
    if (!hGlobal) return FALSE;
    LPBYTE pRawData = LockResource(hGlobal);
    if (!pRawData) return FALSE;

    // Write bom of UTF-16
    WriteFile((HANDLE)lParam, "\xFF\xFE", 2UL, NULL, NULL);

    WCHAR szLicenseNotice[] = L";// Copyright (c) 2022 onein528\r\n;// Licensed under the MIT License.\r\n\r\n";
    //WCHAR szFileHeader[] = L"MessageIdTypedef=DWORD\r\n\r\nSeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS\r\nInformational=0x1:STATUS_SEVERITY_INFORMATIONAL\r\nWarning=0x2:STATUS_SEVERITY_WARNING\r\nError=0x3:STATUS_SEVERITY_ERROR\r\n)\r\n\r\nFacilityNames=(System=0x0:FACILITY_SYSTEM\r\nRuntime=0x2:FACILITY_RUNTIME\r\nStubs=0x3:FACILITY_STUBS\r\nIo=0x4:FACILITY_IO_ERROR_CODE\r\n)\r\n\r\n";

    // Write mc file header
    WriteFile((HANDLE)lParam, szLicenseNotice, wcslen(szLicenseNotice) * sizeof(WCHAR), NULL, NULL);
    //WriteFile((HANDLE)lParam, szFileHeader, wcslen(szFileHeader) * sizeof(WCHAR), NULL, NULL);


    PMESSAGE_RESOURCE_DATA pMsgResData = (PMESSAGE_RESOURCE_DATA)pRawData;
    WCHAR szMessageHeader[256] = L"";

    for (PMESSAGE_RESOURCE_BLOCK MsgResBlock = pMsgResData->Blocks, end = MsgResBlock + pMsgResData->NumberOfBlocks; MsgResBlock < end; ++MsgResBlock) {

        PMESSAGE_RESOURCE_ENTRY pMsgResEntry = (PMESSAGE_RESOURCE_ENTRY)(pRawData + MsgResBlock->OffsetToEntries);


        for (DWORD dwMsgIdIndex = MsgResBlock->LowId; dwMsgIdIndex <= MsgResBlock->HighId; ++dwMsgIdIndex) {

            // Assume "English" message text
            swprintf(szMessageHeader, sizeof(szMessageHeader) / sizeof(WCHAR),
                L"MessageId=0x%08X\r\nSymbolicName=\r\nLanguage=English\r\n", (dwMsgIdIndex << 16) >> 16);

            if (CHECKRETURN(WriteFile((HANDLE)lParam, szMessageHeader, wcslen(szMessageHeader) * sizeof(WCHAR), NULL, NULL)) ||
                CHECKRETURN(WriteFile((HANDLE)lParam, pMsgResEntry->Text, wcslen((LPWSTR)pMsgResEntry->Text) << 1, NULL, NULL)) ||
                CHECKRETURN(WriteFile((HANDLE)lParam, L".\r\n\r\n", 10, NULL, NULL))) {

                return FALSE;
            }

            pMsgResEntry = (PMESSAGE_RESOURCE_ENTRY)((LPBYTE)pMsgResEntry + pMsgResEntry->Length);
        }
    }

    return TRUE;
}
