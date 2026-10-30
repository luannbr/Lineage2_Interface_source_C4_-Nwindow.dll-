// AActor: 198 slots from Engine.dll ??_7AActor@@6B@ (tools/gen_engine_decls.py, do not edit)
ENGINE_API virtual INT* GetOptimizedRepList(BYTE*, FPropertyRetirement*, INT*, UPackageMap*, UActorChannel*);   //  25 +0x064
ENGINE_API virtual APawn* GetPlayerPawn() const;                                                                //  26 +0x068
ENGINE_API virtual INT IsPlayer();                                                                              //  27 +0x06c
ENGINE_API virtual APawn* GetPawn();                                                                            //  28 +0x070
ENGINE_API virtual ALight* GetLight();                                                                          //  29 +0x074
ENGINE_API virtual AEmitter* GetEmitter();                                                                      //  30 +0x078
ENGINE_API virtual FLOAT GetNetPriority(AActor*, FLOAT, FLOAT);                                                 //  31 +0x07c
ENGINE_API virtual FLOAT WorldLightRadius() const;                                                              //  32 +0x080
ENGINE_API virtual INT Tick(FLOAT, ELevelTick);                                                                 //  33 +0x084
ENGINE_API virtual void PostEditMove();                                                                         //  34 +0x088
ENGINE_API virtual void PostEditLoad();                                                                         //  35 +0x08c
ENGINE_API virtual void PreRaytrace();                                                                          //  36 +0x090
ENGINE_API virtual void PostRaytrace();                                                                         //  37 +0x094
ENGINE_API virtual void Spawned();                                                                              //  38 +0x098
ENGINE_API virtual void PreNetReceive();                                                                        //  39 +0x09c
ENGINE_API virtual void PostNetReceive();                                                                       //  40 +0x0a0
ENGINE_API virtual void PostNetReceiveLocation();                                                               //  41 +0x0a4
ENGINE_API virtual UMaterial* GetSkin(INT);                                                                     //  42 +0x0a8
ENGINE_API virtual INT ShouldTickInEntry();                                                                     //  43 +0x0ac
ENGINE_API virtual void L2EventShow(INT);                                                                       //  44 +0x0b0
ENGINE_API virtual void L2EventPlay(INT);                                                                       //  45 +0x0b4
ENGINE_API virtual void LostChild(AActor*);                                                                     //  46 +0x0b8
ENGINE_API virtual void GainedChild(AActor*);                                                                   //  47 +0x0bc
ENGINE_API virtual INT CanBeAttacked();                                                                         //  48 +0x0c0
ENGINE_API virtual INT CanBeTold();                                                                             //  49 +0x0c4
ENGINE_API virtual void PostRender();                                                                           //  50 +0x0c8
ENGINE_API virtual void SetMeshes(INT, FName, INT);                                                             //  51 +0x0cc
ENGINE_API virtual void SetTexes(INT, FName, INT);                                                              //  52 +0x0d0
ENGINE_API virtual UMaterial* GetSubSkin(INT);                                                                  //  53 +0x0d4
ENGINE_API virtual UMaterial* GetRightHandSkin(INT);                                                            //  54 +0x0d8
ENGINE_API virtual UMaterial* GetLeftHandSkin(INT);                                                             //  55 +0x0dc
ENGINE_API virtual UMaterial* GetRightArmSkin(INT);                                                             //  56 +0x0e0
ENGINE_API virtual UMaterial* GetLeftArmSkin(INT);                                                              //  57 +0x0e4
ENGINE_API virtual UMaterial* GetCloakSkin(INT);                                                                //  58 +0x0e8
ENGINE_API virtual void CheckSameAnim();                                                                        //  59 +0x0ec
ENGINE_API virtual INT IsSameAnim();                                                                            //  60 +0x0f0
ENGINE_API virtual void CreateSkin(FRenderInterface*);                                                          //  61 +0x0f4
ENGINE_API virtual void ClearTexModifier();                                                                     //  62 +0x0f8
ENGINE_API virtual void SetAlphaTexModifier(BYTE);                                                              //  63 +0x0fc
ENGINE_API virtual void ClearAlphaTexModifier();                                                                //  64 +0x100
ENGINE_API virtual void SetOverlayTexModifier(BYTE, BYTE, BYTE, BYTE);                                          //  65 +0x104
ENGINE_API virtual void SetOverlayDependAlphaTexModifier(BYTE, BYTE, BYTE);                                     //  66 +0x108
ENGINE_API virtual void SetSubtractTexModifier(BYTE, BYTE, BYTE);                                               //  67 +0x10c
ENGINE_API virtual void SetAddTexModifier(BYTE, BYTE, BYTE);                                                    //  68 +0x110
ENGINE_API virtual void SetChangeColorTexModifier(BYTE, BYTE, BYTE);                                            //  69 +0x114
ENGINE_API virtual INT IsModifiedTexture();                                                                     //  70 +0x118
ENGINE_API virtual UModifier* GetModifiedTextureMaterial();                                                     //  71 +0x11c
ENGINE_API virtual void SetActorViewType(EActorViewType, INT, BYTE);                                            //  72 +0x120
ENGINE_API virtual INT IsNeedTick();                                                                            //  73 +0x124
ENGINE_API virtual void SetStaticMeshActorViewType(INT, INT);                                                   //  74 +0x128
ENGINE_API virtual USound* GetStepSoundData();                                                                  //  75 +0x12c
ENGINE_API virtual FMatrix ConvertOrthMatrix(FMatrix, INT);                                                     //  76 +0x130
ENGINE_API virtual FMatrix CollisionToWorld();                                                                  //  77 +0x134
ENGINE_API virtual FCoords ToLocal() const;                                                                     //  78 +0x138
ENGINE_API virtual FCoords ToWorld() const;                                                                     //  79 +0x13c
ENGINE_API virtual FMatrix LocalToWorld() const;                                                                //  80 +0x140
ENGINE_API virtual FMatrix BillBoardLocalToWorld(FRotator) const;                                               //  81 +0x144
ENGINE_API virtual FMatrix WorldToLocal() const;                                                                //  82 +0x148
ENGINE_API virtual FDynamicLight* GetLightRenderData();                                                         //  83 +0x14c
ENGINE_API virtual void UpdateRenderData();                                                                     //  84 +0x150
ENGINE_API virtual void PostScriptDestroyed();                                                                  //  85 +0x154
ENGINE_API virtual INT ShouldTrace(AActor*, DWORD);                                                             //  86 +0x158
ENGINE_API virtual UPrimitive* GetPrimitive();                                                                  //  87 +0x15c
ENGINE_API virtual void NotifyBump(AActor*);                                                                    //  88 +0x160
ENGINE_API virtual void SetBase(AActor*, FVector, INT);                                                         //  89 +0x164
ENGINE_API virtual void NotifyAnimEnd(INT);                                                                     //  90 +0x168
ENGINE_API virtual void UpdateAnimation(FLOAT);                                                                 //  91 +0x16c
ENGINE_API virtual void StartAnimPoll();                                                                        //  92 +0x170
ENGINE_API virtual INT CheckAnimFinished(INT);                                                                  //  93 +0x174
ENGINE_API virtual INT CheckOwnerUpdated();                                                                     //  94 +0x178
ENGINE_API virtual void TickAuthoritative(FLOAT);                                                               //  95 +0x17c
ENGINE_API virtual void TickSimulated(FLOAT);                                                                   //  96 +0x180
ENGINE_API virtual void TickSpecial(FLOAT);                                                                     //  97 +0x184
ENGINE_API virtual INT PlayerControlled();                                                                      //  98 +0x188
ENGINE_API virtual INT IsNetRelevantFor(APlayerController*, AActor*, FVector);                                  //  99 +0x18c
ENGINE_API virtual void Attacked();                                                                             // 100 +0x190
ENGINE_API virtual INT IsDamageAct();                                                                           // 101 +0x194
ENGINE_API virtual INT IsSpineRotation();                                                                       // 102 +0x198
ENGINE_API virtual FLOAT GetDamageDist();                                                                       // 103 +0x19c
ENGINE_API virtual FVector GetTrailerPrePivot();                                                                // 104 +0x1a0
ENGINE_API virtual void SetTrailerPrePivot(FVector);                                                            // 105 +0x1a4
ENGINE_API virtual void RenderEditorInfo(FLevelSceneNode*, FRenderInterface*, FDynamicActor*);                  // 106 +0x1a8
ENGINE_API virtual void RenderEditorSelected(FLevelSceneNode*, FRenderInterface*, FDynamicActor*);              // 107 +0x1ac
ENGINE_API virtual FLOAT GetAmbientVolume(FLOAT);                                                               // 108 +0x1b0
ENGINE_API virtual void SetZone(INT, INT);                                                                      // 109 +0x1b4
ENGINE_API virtual void SetVolumes();                                                                           // 110 +0x1b8
ENGINE_API virtual void PostBeginPlay();                                                                        // 111 +0x1bc
ENGINE_API virtual void setPhysics(BYTE, AActor*, FVector);                                                     // 112 +0x1c0
ENGINE_API virtual void performPhysics(FLOAT);                                                                  // 113 +0x1c4
ENGINE_API virtual void BoundProjectileVelocity();                                                              // 114 +0x1c8
ENGINE_API virtual void processHitWall(FVector, AActor*);                                                       // 115 +0x1cc
ENGINE_API virtual void processLanded(FVector, AActor*, FLOAT, INT);                                            // 116 +0x1d0
ENGINE_API virtual void physFalling(FLOAT, INT);                                                                // 117 +0x1d4
ENGINE_API virtual FRotator FindSlopeRotation(FVector, FRotator);                                               // 118 +0x1d8
ENGINE_API virtual void SmoothHitWall(FVector, AActor*);                                                        // 119 +0x1dc
ENGINE_API virtual void stepUp(FVector, FVector, FVector, FCheckResult&);                                       // 120 +0x1e0
ENGINE_API virtual INT ShrinkCollision(AActor*);                                                                // 121 +0x1e4
ENGINE_API virtual void L2HitWall(FVector, AActor*);                                                            // 122 +0x1e8
ENGINE_API virtual void L2Touch(AActor*);                                                                       // 123 +0x1ec
ENGINE_API virtual void L2Explode(FVector, AActor*);                                                            // 124 +0x1f0
ENGINE_API virtual USound* GetAttackItemSound(FLOAT&, FLOAT&);                                                  // 125 +0x1f4
ENGINE_API virtual USound* GetDefenseItemSound(FLOAT&, FLOAT&);                                                 // 126 +0x1f8
ENGINE_API virtual USound* GetShieldItemSound(FLOAT&, FLOAT&);                                                  // 127 +0x1fc
ENGINE_API virtual USound* GetDamageSound(FLOAT&, FLOAT&);                                                      // 128 +0x200
ENGINE_API virtual USound* GetAttackVoiceSound(FLOAT);                                                          // 129 +0x204
ENGINE_API virtual void AttackedNotify(APawn*, AActor*, INT, INT, INT);                                         // 130 +0x208
ENGINE_API virtual void AssociateAttackedNotify(APawn*, AActor*, INT, INT, INT, INT, INT, INT, INT, INT, INT);  // 131 +0x20c
ENGINE_API virtual INT IsRendered();                                                                            // 132 +0x210
ENGINE_API virtual FName GetRHandBoneName();                                                                    // 133 +0x214
ENGINE_API virtual FName GetLHandBoneName();                                                                    // 134 +0x218
ENGINE_API virtual FName GetRArmBoneName();                                                                     // 135 +0x21c
ENGINE_API virtual FName GetLArmBoneName();                                                                     // 136 +0x220
ENGINE_API virtual FName GetCapeBoneName();                                                                     // 137 +0x224
ENGINE_API virtual FName GetSpineBoneName();                                                                    // 138 +0x228
ENGINE_API virtual FName GetLowbodyBoneName();                                                                  // 139 +0x22c
ENGINE_API virtual FName GetHeadBoneName();                                                                     // 140 +0x230
ENGINE_API virtual void SetRHandBoneName(FName);                                                                // 141 +0x234
ENGINE_API virtual void SetLHandBoneName(FName);                                                                // 142 +0x238
ENGINE_API virtual void SetRArmBoneName(FName);                                                                 // 143 +0x23c
ENGINE_API virtual void SetLArmBoneName(FName);                                                                 // 144 +0x240
ENGINE_API virtual void SetCapeBoneName(FName);                                                                 // 145 +0x244
ENGINE_API virtual void SetSpineBoneName(FName);                                                                // 146 +0x248
ENGINE_API virtual void SetLowbodyBoneName(FName);                                                              // 147 +0x24c
ENGINE_API virtual void SetHeadBoneName(FName);                                                                 // 148 +0x250
ENGINE_API virtual UMesh* GetSubMesh(INT);                                                                      // 149 +0x254
ENGINE_API virtual UMesh* GetRightHandMesh();                                                                   // 150 +0x258
ENGINE_API virtual UMesh* GetLeftHandMesh();                                                                    // 151 +0x25c
ENGINE_API virtual UMesh* GetRightArmMesh();                                                                    // 152 +0x260
ENGINE_API virtual UMesh* GetLeftArmMesh();                                                                     // 153 +0x264
ENGINE_API virtual UMesh* GetCloakMesh();                                                                       // 154 +0x268
ENGINE_API virtual UMesh* GetCloakCoverMesh();                                                                  // 155 +0x26c
ENGINE_API virtual void SetSubMesh(INT, UMesh*);                                                                // 156 +0x270
ENGINE_API virtual void SetRightHandMesh(UMesh*);                                                               // 157 +0x274
ENGINE_API virtual void SetLeftHandMesh(UMesh*);                                                                // 158 +0x278
ENGINE_API virtual void SetRightArmMesh(UMesh*);                                                                // 159 +0x27c
ENGINE_API virtual void SetLeftArmMesh(UMesh*);                                                                 // 160 +0x280
ENGINE_API virtual void SetCloakMesh(UMesh*);                                                                   // 161 +0x284
ENGINE_API virtual void SetCloakCoverMesh(UMesh*);                                                              // 162 +0x288
ENGINE_API virtual UMeshInstance* GetSubMeshInstance(INT);                                                      // 163 +0x28c
ENGINE_API virtual UMeshInstance* GetRightHandMeshInstance();                                                   // 164 +0x290
ENGINE_API virtual UMeshInstance* GetLeftHandMeshInstance();                                                    // 165 +0x294
ENGINE_API virtual UMeshInstance* GetRightArmMeshInstance();                                                    // 166 +0x298
ENGINE_API virtual UMeshInstance* GetLeftArmMeshInstance();                                                     // 167 +0x29c
ENGINE_API virtual UMeshInstance* GetCloakMeshInstance();                                                       // 168 +0x2a0
ENGINE_API virtual UMeshInstance* GetCloakCoverMeshInstance();                                                  // 169 +0x2a4
ENGINE_API virtual UMeshInstance* GetExtraMeshInstance(INT);                                                    // 170 +0x2a8
ENGINE_API virtual FNMagicInfo* GetMagicInfo();                                                                 // 171 +0x2ac
ENGINE_API virtual void GetTargetLocation(FVector, FVector&);                                                   // 172 +0x2b0
ENGINE_API virtual void physL2Movement(FLOAT, INT);                                                             // 173 +0x2b4
ENGINE_API virtual INT AddMyMarker(AActor*);                                                                    // 174 +0x2b8
ENGINE_API virtual void ClearMarker();                                                                          // 175 +0x2bc
ENGINE_API virtual AActor* AssociatedLevelGeometry();                                                           // 176 +0x2c0
ENGINE_API virtual INT HasAssociatedLevelGeometry(AActor*);                                                     // 177 +0x2c4
ENGINE_API virtual void PlayAnim(INT, FName, FLOAT, FLOAT, INT);                                                // 178 +0x2c8
ENGINE_API virtual void CheckForErrors();                                                                       // 179 +0x2cc
ENGINE_API virtual void PrePath();                                                                              // 180 +0x2d0
ENGINE_API virtual void PostPath();                                                                             // 181 +0x2d4
ENGINE_API virtual AActor* GetProjectorBase();                                                                  // 182 +0x2d8
ENGINE_API virtual INT IsABrush();                                                                              // 183 +0x2dc
ENGINE_API virtual INT IsAMover();                                                                              // 184 +0x2e0
ENGINE_API virtual INT IsAVolume();                                                                             // 185 +0x2e4
ENGINE_API virtual INT IsAPlayerController();                                                                   // 186 +0x2e8
ENGINE_API virtual INT IsAPawn();                                                                               // 187 +0x2ec
ENGINE_API virtual INT IsAProjectile();                                                                         // 188 +0x2f0
ENGINE_API virtual INT IsAAmbientSound();                                                                       // 189 +0x2f4
ENGINE_API virtual INT IsAEmitter();                                                                            // 190 +0x2f8
ENGINE_API virtual INT IsALight();                                                                              // 191 +0x2fc
ENGINE_API virtual INT IsObserverModeActor();                                                                   // 192 +0x300
ENGINE_API virtual INT IsBroadcastObserverModeActor();                                                          // 193 +0x304
ENGINE_API virtual FName GetL2MovementEventName();                                                              // 194 +0x308
ENGINE_API virtual FName GetL2MovementTagName(INT);                                                             // 195 +0x30c
ENGINE_API virtual void AttachL2MovementActor(AActor*);                                                         // 196 +0x310
ENGINE_API virtual APlayerController* GetTopPlayerController();                                                 // 197 +0x314
