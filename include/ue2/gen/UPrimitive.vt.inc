// UPrimitive: 34 slots from Engine.dll ??_7UPrimitive@@6B@ (tools/gen_engine_decls.py, do not edit)
ENGINE_API virtual INT PointCheck(FCheckResult&, AActor*, FVector, FVector, DWORD);                 //  25 +0x064
ENGINE_API virtual INT LineCheck(FCheckResult&, AActor*, FVector, FVector, FVector, DWORD, DWORD);  //  26 +0x068
ENGINE_API virtual FBox GetRenderBoundingBox(const AActor*);                                        //  27 +0x06c
ENGINE_API virtual FSphere GetRenderBoundingSphere(const AActor*);                                  //  28 +0x070
ENGINE_API virtual FBox GetCollisionBoundingBox(const AActor*) const;                               //  29 +0x074
ENGINE_API virtual INT UseCylinderCollision(const AActor*);                                         //  30 +0x078
ENGINE_API virtual void Illuminate(AActor*, INT);                                                   //  31 +0x07c
ENGINE_API virtual FVector GetEncroachExtent(AActor*);                                              //  32 +0x080
ENGINE_API virtual FVector GetEncroachCenter(AActor*);                                              //  33 +0x084
