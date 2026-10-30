// UViewport: 48 slots from Engine.dll ??_7UViewport@@6BUObject@@@ (tools/gen_engine_decls.py, do not edit)
ENGINE_API virtual INT Lock(BYTE*, INT*);                              //  26 +0x068
ENGINE_API virtual void Unlock();                                      //  27 +0x06c
ENGINE_API virtual void Present();                                     //  28 +0x070
ENGINE_API virtual INT SetDrag(INT);                                   //  29 +0x074
virtual INT IsFullscreen() = 0;                                        //  30 +0x078
virtual INT ResizeViewport(DWORD, INT, INT, INT) = 0;                  //  31 +0x07c
virtual void SetModeCursor() = 0;                                      //  32 +0x080
virtual void UpdateWindowFrame() = 0;                                  //  33 +0x084
virtual void OpenWindow(DWORD, INT, INT, INT, INT, INT) = 0;           //  34 +0x088
virtual void CloseWindow() = 0;                                        //  35 +0x08c
virtual void UpdateInput(INT, FLOAT) = 0;                              //  36 +0x090
virtual void* GetWindow() = 0;                                         //  37 +0x094
ENGINE_API virtual void* GetServer();                                  //  38 +0x098
virtual void SetMouseCapture(INT, INT, INT) = 0;                       //  39 +0x09c
virtual void Repaint(INT) = 0;                                         //  40 +0x0a0
ENGINE_API virtual void TryRenderDevice(const TCHAR*, INT, INT, INT);  //  41 +0x0a4
virtual TCHAR* GetLocalizedKeyName(EInputKey) = 0;                     //  42 +0x0a8
virtual void PlayForceFeedBack(INT, FLOAT) = 0;                        //  43 +0x0ac
virtual void TickForceFeedBack(FLOAT) = 0;                             //  44 +0x0b0
virtual void SetMouseDisable(INT) = 0;                                 //  45 +0x0b4
virtual INT IsDefaultConsolePos() = 0;                                 //  46 +0x0b8
ENGINE_API virtual INT IsMouseCaptured();                              //  47 +0x0bc
