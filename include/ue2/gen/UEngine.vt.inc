// UEngine: 49 slots from Engine.dll ??_7UEngine@@6BUObject@@@ (tools/gen_engine_decls.py, do not edit)
ENGINE_API virtual void Init(INT);                                                   //  26 +0x068
ENGINE_API virtual void Exit();                                                      //  27 +0x06c
ENGINE_API virtual void Flush(INT);                                                  //  28 +0x070
ENGINE_API virtual void UpdateGamma();                                               //  29 +0x074
ENGINE_API virtual void RestoreGamma();                                              //  30 +0x078
ENGINE_API virtual INT Key(UViewport*, EInputKey, _WORD);                            //  31 +0x07c
ENGINE_API virtual INT InputEvent(UViewport*, EInputKey, EInputAction, FLOAT, INT);  //  32 +0x080
virtual void Draw(UViewport*, INT, BYTE*, INT*) = 0;                                 //  33 +0x084
virtual void MouseDelta(UViewport*, DWORD, FLOAT, FLOAT) = 0;                        //  34 +0x088
virtual void MousePosition(UViewport*, DWORD, FLOAT, FLOAT) = 0;                     //  35 +0x08c
virtual void MouseWheel(UViewport*, DWORD, INT) = 0;                                 //  36 +0x090
virtual void Click(UViewport*, DWORD, FLOAT, FLOAT) = 0;                             //  37 +0x094
virtual void UnClick(UViewport*, DWORD, INT, INT) = 0;                               //  38 +0x098
virtual void SetClientTravel(UPlayer*, const TCHAR*, INT, ETravelType) = 0;          //  39 +0x09c
ENGINE_API virtual INT ChallengeResponse(INT);                                       //  40 +0x0a0
ENGINE_API virtual FLOAT GetMaxTickRate();                                           //  41 +0x0a4
ENGINE_API virtual void SetProgress(const TCHAR*, const TCHAR*, FLOAT);              //  42 +0x0a8
ENGINE_API virtual void edSetClickLocation(FVector&);                                //  43 +0x0ac
ENGINE_API virtual void edDrawAxisIndicator(FSceneNode*);                            //  44 +0x0b0
ENGINE_API virtual INT edcamMode(UViewport*);                                        //  45 +0x0b4
ENGINE_API virtual INT edcamTerrainBrush();                                          //  46 +0x0b8
ENGINE_API virtual INT edcamMouseControl(UViewport*);                                //  47 +0x0bc
ENGINE_API virtual void EdCallback(DWORD, INT, DWORD);                               //  48 +0x0c0
