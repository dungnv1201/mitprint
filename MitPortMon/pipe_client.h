#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "protocol.h"

// Connect to MitPrint.exe named pipe. Returns INVALID_HANDLE_VALUE on failure.
HANDLE MitPipeConnect();

// Send header + optional payload
BOOL MitPipeSend(HANDLE hPipe, const MitMsgHeader* pHdr,
                 const void* pPayload, DWORD cbPayload);

// Send header + DataChunkPayload + raw data in one write
BOOL MitPipeSendChunk(HANDLE hPipe, const MitMsgHeader* pHdr,
                      const MitDataChunkPayload* pChunk,
                      const BYTE* pData, DWORD cbData);
