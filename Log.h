#pragma once
#include <stdarg.h>
#include <strsafe.h> 
#include <Windows.h>


#define dfLOG_LEVEL_DEBUG 0
#define dfLOG_LEVEL_ERROR 1
#define dfLOG_LEVEL_SYSTEM 2

#define LOG_BUFFER_LEN 1024


//----------------------------------------------
// �α� ��� ���
// 1. �ܼ�
//  - _LOG(dfLOG_LEVEL_~, L"~") ��ũ�θ� �̿��մϴ�.
// 2. ��� ���� ����
//  - _LOG(dfLOG_LEVEL_~, L"~") ��ũ�θ� �̿��մϴ�.
//----------------------------------------------
#define _LOG(Level, ...)               \
do{                                    \
    Log((Level), __VA_ARGS__);         \
} while (0)                            \


enum ELogMode
{
    NOLOG = 0,
    CONSOLE,
    FILE_DIRECT,
};


//----------------------------------------------
// �α� ��� ��忡 ���� ���ҽ� �ʱ�ȭ
//----------------------------------------------
bool InitLog(int logLevel, ELogMode logMode);

//----------------------------------------------
// �α� ��� ��忡 ���� �α� ���
//----------------------------------------------
void Log(int level, const wchar_t* fmt, ...);


bool CloseLog();