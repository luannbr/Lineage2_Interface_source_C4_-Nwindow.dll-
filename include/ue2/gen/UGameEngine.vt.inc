// UGameEngine: 58 slots from Engine.dll ??_7UGameEngine@@6BUObject@@@ (tools/gen_engine_decls.py, do not edit)
ENGINE_API virtual INT Browse(FURL, const TMap<FString, FString>*, FString&);                              //  49 +0x0c4
ENGINE_API virtual ULevel* LoadMap(const FURL&, UPendingLevel*, const TMap<FString, FString>*, FString&);  //  50 +0x0c8
ENGINE_API virtual void SaveGame(INT);                                                                     //  51 +0x0cc
ENGINE_API virtual void CancelPending();                                                                   //  52 +0x0d0
ENGINE_API virtual void PaintProgress();                                                                   //  53 +0x0d4
ENGINE_API virtual void UpdateConnectingMessage();                                                         //  54 +0x0d8
ENGINE_API virtual void BuildServerMasterMap(UNetDriver*, ULevel*);                                        //  55 +0x0dc
ENGINE_API virtual void NotifyLevelChange();                                                               //  56 +0x0e0
ENGINE_API virtual void ContinueServerClose();                                                             //  57 +0x0e4
