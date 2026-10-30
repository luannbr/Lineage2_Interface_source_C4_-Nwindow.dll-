// URenderDevice: 54 slots from Engine.dll ??_7URenderDevice@@6BUObject@@@ (tools/gen_engine_decls.py, do not edit)
virtual INT Init() = 0;                                             //  26 +0x068
virtual INT SetRes(UViewport*, INT, INT, INT, INT, INT) = 0;        //  28 +0x070
virtual INT SetRes(UViewport*, INT, INT, INT, INT, INT, INT) = 0;   //  27 +0x06c
virtual INT GetRes(INT, INT*, INT*, INT*) = 0;                      //  29 +0x074
virtual INT GetRefreshRate(INT, INT, INT, INT, INT*) = 0;           //  30 +0x078
virtual void Exit(UViewport*) = 0;                                  //  31 +0x07c
virtual void Flush(UViewport*) = 0;                                 //  32 +0x080
virtual void FlushResource(QWORD) = 0;                              //  33 +0x084
ENGINE_API virtual INT ResourceCached(QWORD);                       //  34 +0x088
virtual void UpdateGamma(UViewport*) = 0;                           //  35 +0x08c
virtual void RestoreGamma() = 0;                                    //  36 +0x090
virtual FRenderInterface* Lock(UViewport*, BYTE*, INT*) = 0;        //  37 +0x094
virtual void Unlock(FRenderInterface*) = 0;                         //  38 +0x098
virtual void Present(UViewport*) = 0;                               //  39 +0x09c
virtual void ReadPixels(UViewport*, FColor*) = 0;                   //  40 +0x0a0
virtual void HoldBackBuffer() = 0;                                  //  41 +0x0a4
virtual void DeleteAllTexture() = 0;                                //  42 +0x0a8
virtual INT IsUseTrilinear() = 0;                                   //  43 +0x0ac
virtual void SetUseTrilinear(INT) = 0;                              //  44 +0x0b0
virtual DWORD GetCreateTextureBytes() = 0;                          //  45 +0x0b4
virtual void SetEmulationMode(EHardwareEmulationMode) = 0;          //  46 +0x0b8
virtual FRenderCaps* GetRenderCaps() = 0;                           //  47 +0x0bc
virtual void ReLoadPixelShader() = 0;                               //  48 +0x0c0
ENGINE_API virtual INT GetResParams(INT&, INT&, INT&, INT&, INT&);  //  49 +0x0c4
ENGINE_API virtual void GetShaderVersion(INT&, INT&);               //  50 +0x0c8
ENGINE_API virtual INT GetMultiSample();                            //  51 +0x0cc
ENGINE_API virtual INT SupportsTextureFormat(ETextureFormat);       //  52 +0x0d0
virtual FRenderInterface* GetRenderInterface() = 0;                 //  53 +0x0d4
