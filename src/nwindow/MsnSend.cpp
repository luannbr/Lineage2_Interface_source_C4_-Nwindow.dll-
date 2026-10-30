/*=============================================================================
    MsnSend.cpp - the senders of the messenger's connections (C4 addresses): the text of a command (CRLF ended, in
    UTF-8) and a payload (as it is, in the ANSI code page) written to the socket. Stock: 0x101607E0, 0x10160950, 0x10160A60. They are
    members of MSNSocket (MSNSocket.h: Send / SendPayload call SendLine / SendRaw on a copy); the by-value command is an
    NMsnLocalCommand, whose destructor is the stock's Release (0x1014EBB0) that the callee runs at its end.
=============================================================================*/
#include "nwindow/MSNSocket.h"
#include "nwindow/MsnCommand.h"
#include "nwindow/MsnUtil.h"
#include "nwindow/NChatMsg.h"
#include "nwindow/MsnStockArgs.h"

#if !NW_HOOK_MODE
#define _WINSOCK_DEPRECATED_NO_WARNINGS   // the stock calls WSAAsyncSelect, inet_addr, gethostbyname
#include <winsock2.h>   // the standalone imports ws2_32 itself
#endif
#include <string.h>

// winsock through the stock image's own import thunk (stdcall; the hook DLL does not import ws2_32)
static FORCEINLINE INT MsnSend(UINT Socket, const char* Buffer, INT Length, INT Flags)    // thunk 0x1014C174
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)(UINT, const char*, INT, INT);
    return ((Fn)::nw::Addr(0x10170B32))(Socket, Buffer, Length, Flags);   // C4: 0x10170B44 is closesocket
#else
    return send(Socket, Buffer, Length, Flags);
#endif
}

// 0x101607e0 (ret 0x1C) - a command with something in it: its wire text and CR LF, converted to UTF-8 (0x10168C60), is sent; a short
// write reads the socket's error (nothing is done with it). Whether it had anything or not, the transaction counter goes up and the
// command is released
void MSNSocket::SendLine(NMsnLocalCommand Command)
{
    if (Command.GetLength() > 0)
    {
        FString Text;
        {
            FString Line = Command.ToString();
            Text = Line + TEXT("\r\n");
        }
        char Buffer[0x410];
        appMemset(Buffer, 0, 0x410);
        NUtf8Encode(*Text, Buffer);
        INT Length = (INT)strlen(Buffer);
        if (MsnSend(Sock.hSock, Buffer, Length, 0) != (INT)strlen(Buffer))
            MsnWSAGetLastError();
    }
    NextTrID++;
}
NW_HOOK_VARARGS(0x101607e0, MSNSocket, SendLine, void (MSNSocket::*)(NMsnLocalCommand))

// 0x10160950 (ret 0x1C) - the same for a payload: the wire text without CR LF, in the ANSI code page (0x10168BE0)
void MSNSocket::SendRaw(NMsnLocalCommand Command)
{
    if (Command.GetLength() > 0)
    {
        FString Text = Command.ToString();
        char Buffer[0x410];
        appMemset(Buffer, 0, 0x410);
        MsnWideToAnsi(*Text, Buffer);
        INT Length = (INT)strlen(Buffer);
        if (MsnSend(Sock.hSock, Buffer, Length, 0) != (INT)strlen(Buffer))
            MsnWSAGetLastError();
    }
    NextTrID++;
}
NW_HOOK_VARARGS(0x10160950, MSNSocket, SendRaw, void (MSNSocket::*)(NMsnLocalCommand))

// 0x10160a60 (ret 0x74: the command 0x1C + the message 0x58, both by value) - a command followed by a message: the message's payload
// (NMsnMessage::Serialize 0x1014C450: its headers and text) converted to UTF-8 (0x10168C60) gives the byte count, which becomes the
// command's last argument (AddIntArg 0x1014E2E0); the command's wire text (0x1014BEF0), CR LF and the payload are joined, converted and sent in one
// write (a short write reads the socket's error); the transaction counter goes up. The callee frees both arguments: the command (released
// as the stock does) and then the message (0x1014EFE0, MsnMessageFree). The message is plain bytes (NMsnMessageArg) so that the compiler runs
// no destructor of its own on it. C4 against C3: the same code, only the addresses moved
void MSNSocket::SendMessageBody(NMsnLocalCommand Command, NMsnMessageArg Message)
{
    {
        FString Payload = Message.Msg().Serialize();
        FString Text;
        char Buffer[0x1040];
        appMemset(Buffer, 0, 0x1040);
        NUtf8Encode(*Payload, Buffer);
        INT Length = (INT)strlen(Buffer);
        Command.AddIntArg(Length);
        {
            FString Line = Command.ToString();
            Text = Line;
        }
        Text += TEXT("\r\n");
        Text += Payload;
        appMemset(Buffer, 0, 0x1040);
        NUtf8Encode(*Text, Buffer);
        if (MsnSend(Sock.hSock, Buffer, (INT)strlen(Buffer), 0) != (INT)strlen(Buffer))
            MsnWSAGetLastError();
        NextTrID++;
    }
    MsnMessageFree(Message.Raw);
}
typedef void (MSNSocket::*SigSendMessageBody)(NMsnLocalCommand, NMsnMessageArg);
NW_HOOK_VARARGS(0x10160a60, MSNSocket, SendMessageBody, SigSendMessageBody)
