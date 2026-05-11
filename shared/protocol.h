#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ---------------------------------------------------------------
// Named pipe IPC protocol between MitPortMon.dll and MitPrint.exe
// ---------------------------------------------------------------

enum class MitMsgType : UINT32
{
    StartJob  = 1,   // DLL -> App: new print job starting
    DataChunk = 2,   // DLL -> App: raw EMF spool data chunk
    EndJob    = 3,   // DLL -> App: all data sent
    CancelJob = 4,   // App -> DLL: cancel in-progress job
    Ack       = 5,   // bidirectional acknowledgement
};

#pragma pack(push, 1)

struct MitMsgHeader
{
    UINT32     magic;        // 0x4D495450 ('MITP')
    UINT32     version;      // protocol version, currently 1
    MitMsgType type;
    UINT32     payloadSize;  // bytes following this header
    UINT32     jobId;
    UINT32     sequence;     // monotonic counter per job
};

struct MitStartJobPayload
{
    WCHAR  documentName[260];
    WCHAR  userName[64];
    DWORD  totalPages;       // 0 if unknown
    DWORD  flags;
};

struct MitDataChunkPayload
{
    DWORD  offset;           // byte offset within full spool stream
    DWORD  chunkSize;        // bytes that follow this struct
    // BYTE data[chunkSize] immediately follows
};

struct MitEndJobPayload
{
    DWORD  totalBytesWritten;
    DWORD  result;           // 0 = success
};

struct MitAckPayload
{
    UINT32 ackSequence;
    DWORD  result;
};

#pragma pack(pop)

// ---------------------------------------------------------------
// Shared constants
// ---------------------------------------------------------------

constexpr WCHAR MIT_PIPE_NAME[]    = L"\\\\.\\pipe\\MitPrintPort";
constexpr WCHAR MIT_PORT_NAME[]    = L"MitPort:";
constexpr WCHAR MIT_MONITOR_NAME[] = L"Mit Print Monitor";
constexpr WCHAR MIT_PRINTER_NAME[] = L"MitPrint";
constexpr WCHAR MIT_DRIVER_NAME[]  = L"MitPrint Driver";
constexpr WCHAR MIT_MUTEX_NAME[]   = L"Global\\MitPrintSingleInstance";
constexpr UINT32 MIT_MSG_MAGIC     = 0x4D495450;
constexpr UINT32 MIT_PROTOCOL_VER  = 1;
constexpr DWORD  MIT_PIPE_TIMEOUT  = 5000;  // ms
constexpr DWORD  MIT_PIPE_BUF_SIZE = 65536;
