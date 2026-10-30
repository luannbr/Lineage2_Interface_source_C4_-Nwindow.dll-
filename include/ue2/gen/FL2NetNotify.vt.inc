// FL2NetNotify: 430 slots from Engine.dll ??_7FL2NetNotify@@6B@ (tools/gen_engine_decls.py, do not edit)
virtual INT OnVersionCheck();                                                                            //   0 +0x000
virtual INT OnMoveToLocation(User*, FVector, FVector);                                                   //   1 +0x004
virtual INT OnMoveToLocationInVehicle(FVehicle*, User*, FVector, FVector);                               //   2 +0x008
virtual INT OnMoveBackwardToLocation(User*, FVector, FVector);                                           //   3 +0x00c
virtual INT OnMoveToPawn(User*, User*, INT, FVector);                                                    //   4 +0x010
virtual INT OnValidateLocation(User*, FVector, INT);                                                     //   5 +0x014
virtual INT OnCheckGeo(FVector);                                                                         //   6 +0x018
virtual INT OnValidateLocationInVehicle(FVehicle*, User*, FVector, INT);                                 //   7 +0x01c
virtual INT OnFlyToLocation(User*, FVector, FVector);                                                    //   8 +0x020
virtual INT OnEarthQuake(FVector, INT, INT, INT);                                                        //   9 +0x024
virtual INT OnStartRotating(User*, INT, INT, INT);                                                       //  10 +0x028
virtual INT OnFinishRotating(User*, INT, INT);                                                           //  11 +0x02c
virtual INT OnNpcSay(L2ParamStack&);                                                                     //  12 +0x030
virtual INT OnCharInfo(User*, FVector, INT, L2ParamStack&);                                              //  13 +0x034
virtual INT OnUserInfo(User*, FVector, INT, INT, INT, ANSICHAR);                                         //  14 +0x038
virtual INT OnAttack(User*, User*, INT, INT, INT, INT, INT, FVector, INT);                               //  16 +0x040
virtual INT OnAttack(User*, INT, INT, INT, INT, INT, FVector, INT);                                      //  15 +0x03c
virtual INT OnAttacked(User*, User*);                                                                    //  18 +0x048
virtual INT OnAttacked(User*, INT);                                                                      //  17 +0x044
virtual INT OnAttackCanceled(User*);                                                                     //  19 +0x04c
virtual INT OnDie(User*, L2ParamStack&);                                                                 //  20 +0x050
virtual INT OnRide(User*, INT, INT, INT);                                                                //  21 +0x054
virtual INT OnReviveRequest(INT, INT);                                                                   //  22 +0x058
virtual INT OnRevive(User*);                                                                             //  23 +0x05c
virtual INT OnAttackOutofRange(User*);                                                                   //  24 +0x060
virtual INT OnAttackinCoolTime(User*);                                                                   //  25 +0x064
virtual INT OnAttackDeadTarget(User*);                                                                   //  26 +0x068
virtual INT OnLeaveWorld(User*);                                                                         //  27 +0x06c
virtual INT OnAuthLoginSuccess();                                                                        //  28 +0x070
virtual INT OnStartServerList(INT);                                                                      //  29 +0x074
virtual INT OnAddServerList(INT, TCHAR*, INT, INT, INT, INT);                                            //  31 +0x07c
virtual INT OnAddServerList(L2ParamStack&);                                                              //  30 +0x078
virtual INT OnEndServerList(INT);                                                                        //  32 +0x080
virtual INT OnAuthLoginFail(INT);                                                                        //  33 +0x084
virtual INT OnBlockedAccountWithMsg(TCHAR*);                                                             //  34 +0x088
virtual INT OnBlockedAccount(INT);                                                                       //  35 +0x08c
virtual INT OnCharacterCreateFail(INT);                                                                  //  36 +0x090
virtual INT OnAuthServerSelectSuccess();                                                                 //  37 +0x094
virtual INT OnAuthServerSelectFail(INT);                                                                 //  38 +0x098
virtual INT OnSpawnItem(Item*, FVector, FRotator);                                                       //  39 +0x09c
virtual INT OnDropItem(User*, Item*, FVector, FRotator);                                                 //  40 +0x0a0
virtual INT OnDice(User*, INT, INT, FVector);                                                            //  41 +0x0a4
virtual INT OnEquipItemPlaySound(INT);                                                                   //  42 +0x0a8
virtual INT OnGetItem(User*, Item*, FVector);                                                            //  43 +0x0ac
virtual INT OnEquipItem(User*, Item*, INT);                                                              //  44 +0x0b0
virtual INT OnUnequipItem(User*, Item*, INT);                                                            //  45 +0x0b4
virtual INT OnClassUpdate(User*, INT);                                                                   //  46 +0x0b8
virtual INT OnLevelUpdate(User*, INT);                                                                   //  47 +0x0bc
virtual INT OnExpUpdate(User*, INT);                                                                     //  48 +0x0c0
virtual INT OnStrUpdate(User*, INT);                                                                     //  49 +0x0c4
virtual INT OnDexUpdate(User*, INT);                                                                     //  50 +0x0c8
virtual INT OnConUpdate(User*, INT);                                                                     //  51 +0x0cc
virtual INT OnIntUpdate(User*, INT);                                                                     //  52 +0x0d0
virtual INT OnWitUpdate(User*, INT);                                                                     //  53 +0x0d4
virtual INT OnMenUpdate(User*, INT);                                                                     //  54 +0x0d8
virtual INT OnHpUpdate(User*, INT);                                                                      //  55 +0x0dc
virtual INT OnMaxHpUpdate(User*, INT);                                                                   //  56 +0x0e0
virtual INT OnCpUpdate(User*, INT);                                                                      //  57 +0x0e4
virtual INT OnMaxCpUpdate(User*, INT);                                                                   //  58 +0x0e8
virtual INT OnMpUpdate(User*, INT);                                                                      //  59 +0x0ec
virtual INT OnMaxMpUpdate(User*, INT);                                                                   //  60 +0x0f0
virtual INT OnSPUpdate(User*, INT);                                                                      //  61 +0x0f4
virtual INT OnCarringWeightUpdate(User*, INT);                                                           //  62 +0x0f8
virtual INT OnCarryWeightUpdate(User*, INT);                                                             //  63 +0x0fc
virtual INT OnAttackRangeUpdate(User*, INT);                                                             //  64 +0x100
virtual INT OnPhysicalAttack(User*, INT);                                                                //  65 +0x104
virtual INT OnPhysicalAttackSpeed(User*, INT);                                                           //  66 +0x108
virtual INT OnPhysicalDefense(User*, INT);                                                               //  67 +0x10c
virtual INT OnPhysicalAvoid(User*, INT);                                                                 //  68 +0x110
virtual INT OnHitRate(User*, INT);                                                                       //  69 +0x114
virtual INT OnCriticalRate(User*, INT);                                                                  //  70 +0x118
virtual INT OnMagicalAttack(User*, INT);                                                                 //  71 +0x11c
virtual INT OnMagicCastingSpeed(User*, INT);                                                             //  72 +0x120
virtual INT OnMagicDefense(User*, INT);                                                                  //  73 +0x124
virtual INT OnGuiltyUpdate(User*, INT);                                                                  //  74 +0x128
virtual INT OnCriminalRateUpdate(User*, INT);                                                            //  75 +0x12c
virtual INT OnNpcHtmlMessage(User*, TCHAR*, INT);                                                        //  76 +0x130
virtual INT OnTutorialShowHtml(TCHAR*);                                                                  //  77 +0x134
virtual INT OnTutorialShowQuestionMark(INT);                                                             //  78 +0x138
virtual INT OnTutorialEnableClientEvent(INT);                                                            //  79 +0x13c
virtual INT OnTutorialCloseHtml();                                                                       //  80 +0x140
virtual INT CheckTutorialClientEvent(INT);                                                               //  81 +0x144
virtual INT OnShowRadar(INT, INT, INT, INT, INT);                                                        //  82 +0x148
virtual INT OnDeleteRadar(INT, INT, INT);                                                                //  83 +0x14c
virtual INT OnStartSellList(INT, INT);                                                                   //  84 +0x150
virtual INT OnAddSellList(ItemInfo&);                                                                    //  85 +0x154
virtual INT OnStartBuyList(INT, INT);                                                                    //  86 +0x158
virtual INT OnAddBuyList(ItemInfo&);                                                                     //  87 +0x15c
virtual INT OnStartPreviewList(INT, INT, INT);                                                           //  88 +0x160
virtual INT OnAddPreviewList(ItemInfo&);                                                                 //  89 +0x164
virtual INT OnPreviewInfo(INT*);                                                                         //  90 +0x168
virtual INT OnStartManorSellList(INT, INT);                                                              //  91 +0x16c
virtual INT OnAddManorSellList(ItemInfo&);                                                               //  92 +0x170
virtual INT OnStartManorBuyList(INT, INT);                                                               //  93 +0x174
virtual INT OnAddManorBuyList(ItemInfo&);                                                                //  94 +0x178
virtual INT OnStartDepositList(SWORD, INT);                                                              //  95 +0x17c
virtual INT OnAddDepositList(ItemInfo&);                                                                 //  96 +0x180
virtual INT OnStartWithdrawList(SWORD, INT);                                                             //  97 +0x184
virtual INT OnAddWithdrawList(ItemInfo&);                                                                //  98 +0x188
virtual INT OnDeleteUser(User*, INT, INT);                                                               //  99 +0x18c
virtual INT OnDeleteItem(Item*);                                                                         // 100 +0x190
virtual INT OnGMHide(INT);                                                                               // 101 +0x194
virtual INT OnServerObjectInfo(User*, FVector, INT, INT, INT);                                           // 102 +0x198
virtual INT OnNpcInfo(User*, FVector, INT, L2ParamStack&);                                               // 103 +0x19c
virtual INT OnActionFail();                                                                              // 104 +0x1a0
virtual INT OnTeleportToLocation(User*, FVector, INT);                                                   // 105 +0x1a4
virtual INT OnTargetSelected(User*, INT, User*, Item*, FServerStaticObject*, FVector);                   // 106 +0x1a8
virtual INT OnMyTargetSelected(User*, INT, Item*, FServerStaticObject*, INT);                            // 107 +0x1ac
virtual INT OnTargetUnselected(User*, FVector);                                                          // 108 +0x1b0
virtual INT OnAutoAttackStart(User*);                                                                    // 109 +0x1b4
virtual INT OnAutoAttackStop(User*);                                                                     // 110 +0x1b8
virtual INT OnSocialAction(User*, INT);                                                                  // 111 +0x1bc
virtual INT OnChangeMoveType(User*, INT, INT);                                                           // 112 +0x1c0
virtual INT OnChangeWaitType(User*, INT, FVector);                                                       // 113 +0x1c4
virtual INT OnStopMove(User*, FVector, INT);                                                             // 114 +0x1c8
virtual INT OnStopMoveInVehicle(FVehicle*, User*, FVector, INT);                                         // 115 +0x1cc
virtual INT OnStopMoveWithLocation(User*, FVector);                                                      // 116 +0x1d0
virtual INT OnDoorInfo(L2ParamStack&, AActor**);                                                         // 117 +0x1d4
virtual INT OnDoorStatusUpdate(L2ParamStack&, AActor*);                                                  // 118 +0x1d8
virtual INT OnVehicleInfo(FVehicle*, FVector, INT);                                                      // 119 +0x1dc
virtual INT OnDeleteVehicle(FVehicle*);                                                                  // 120 +0x1e0
virtual INT OnVehicleDeparture(FVehicle*, INT, INT, FVector);                                            // 121 +0x1e4
virtual INT OnVehicleCheckLocation(FVehicle*, FVector, INT);                                             // 122 +0x1e8
virtual INT OnGetOnVehicle(User*, FVehicle*, FVector);                                                   // 123 +0x1ec
virtual INT OnGetOffVehicle(User*, FVehicle*, FVector);                                                  // 124 +0x1f0
virtual INT OnSpecialCamera(User*, FLOAT, INT, INT, FLOAT, FLOAT, INT, INT, INT, INT);                   // 125 +0x1f4
virtual INT OnNormalCamera();                                                                            // 126 +0x1f8
virtual INT OnEventTrigger(INT, INT);                                                                    // 127 +0x1fc
virtual INT OnSkillRemainSec(User*, User*, INT, INT, INT, INT, FLOAT);                                   // 128 +0x200
virtual void OnAcceptLogOn();                                                                            // 129 +0x204
virtual void OnAcceptLogOut();                                                                           // 130 +0x208
virtual INT CheckLoginType(INT);                                                                         // 131 +0x20c
virtual void OnStartNotice(L2ParamStack&);                                                               // 132 +0x210
virtual void OnSCCheckStart(INT, INT);                                                                   // 133 +0x214
virtual void OnReceiveCharacterInfo(INT);                                                                // 134 +0x218
virtual void AddCharacterInfo(User);                                                                     // 135 +0x21c
virtual void OnAcceptGameStart(INT);                                                                     // 136 +0x220
virtual void OnAcceptCharacterDelete(INT);                                                               // 137 +0x224
virtual void OnAcceptNewCharacter(INT);                                                                  // 138 +0x228
virtual void OnAcceptCharacterCreate(INT);                                                               // 139 +0x22c
virtual void OnReceiveItemList(INT);                                                                     // 140 +0x230
virtual void OnReceiveUpdateItemList(INT, ItemInfo&);                                                    // 141 +0x234
virtual void OnReceiveTradeStart(INT);                                                                   // 142 +0x238
virtual void OnReceiveAddTradeInventoryItem(ItemInfo&);                                                  // 143 +0x23c
virtual void OnReceiveAddTradeOwnItem(ItemInfo&);                                                        // 144 +0x240
virtual void OnReceiveAddTradeOtherItem(ItemInfo&);                                                      // 145 +0x244
virtual void OnReceiveTradeUpdate(INT, ItemInfo&);                                                       // 146 +0x248
virtual void OnReceiveTradeDoneMsg(INT);                                                                 // 147 +0x24c
virtual void OnReceiveCharacterSelectedPacket(INT, INT, INT, INT, void*, INT, INT*);                     // 148 +0x250
virtual void AddRecommandedData(RecommandedDataStr);                                                     // 149 +0x254
virtual void EndRecommandedData();                                                                       // 150 +0x258
virtual void AddInventoryItem(ItemInfo&);                                                                // 151 +0x25c
virtual void OnUpdatePlayerInfo(L2ParamStack&);                                                          // 152 +0x260
virtual void OnServerClose(INT);                                                                         // 153 +0x264
virtual void OnReceiveHtmlMessage(L2ParamStack&);                                                        // 154 +0x268
virtual void OnReceivePledgeExtendedInfo(PledgeInfo&);                                                   // 155 +0x26c
virtual void OnReceivePledgeInfo(PledgeInfo&);                                                           // 156 +0x270
virtual void OnReceivePledgeInfoUpdate(PledgeInfo&);                                                     // 157 +0x274
virtual void OnReceivePledgeMemberList();                                                                // 158 +0x278
virtual void OnAddPledgeMember(L2ParamStack&);                                                           // 159 +0x27c
virtual void OnUpdatePledgeMember(L2ParamStack&);                                                        // 160 +0x280
virtual void OnDeletePledgeMember(TCHAR*);                                                               // 161 +0x284
virtual void OnDeleteAllPledgeMember();                                                                  // 162 +0x288
virtual void OnReceiveCreatePledge(L2ParamStack&);                                                       // 163 +0x28c
virtual void OnImportCrestFile(INT, INT, INT, INT);                                                      // 164 +0x290
virtual void OnMakeDDSCrestFile(INT, INT, BYTE*, INT, INT);                                              // 165 +0x294
virtual void OnReceiveAskJoinPledge(L2ParamStack&);                                                      // 166 +0x298
virtual void OnReceiveOustPledgeMember(L2ParamStack&);                                                   // 167 +0x29c
virtual void OnReceiveDismissPledge(L2ParamStack&);                                                      // 168 +0x2a0
virtual void OnReceiveJoinPledge(L2ParamStack&);                                                         // 169 +0x2a4
virtual void OnReceiveWidthdrawalPledge(L2ParamStack&);                                                  // 170 +0x2a8
virtual void OnReceiveOustPledgeMessage(L2ParamStack&);                                                  // 171 +0x2ac
virtual void OnReceiveDismissPledgeMessage(L2ParamStack&);                                               // 172 +0x2b0
virtual void OnReceiveStartPledgeWar(L2ParamStack&);                                                     // 173 +0x2b4
virtual void OnReceiveReplyStartPledgeWar(L2ParamStack&);                                                // 174 +0x2b8
virtual void OnReceiveStopPledgeWar(L2ParamStack&);                                                      // 175 +0x2bc
virtual void OnReceiveReplyStopPledgeWar(L2ParamStack&);                                                 // 176 +0x2c0
virtual void OnReceiveSurrenderPledgeWar(L2ParamStack&);                                                 // 177 +0x2c4
virtual void OnReceiveReplySurrenderPledgeWar(L2ParamStack&);                                            // 178 +0x2c8
virtual void OnReceiveSurrenderPersonally(L2ParamStack&);                                                // 179 +0x2cc
virtual void OnReceiveAskJoinAlly(L2ParamStack&);                                                        // 180 +0x2d0
virtual void OnReceiveOustAlly(L2ParamStack&);                                                           // 181 +0x2d4
virtual void OnReceiveDismissAlly(L2ParamStack&);                                                        // 182 +0x2d8
virtual void OnReceiveJoinAlly(L2ParamStack&);                                                           // 183 +0x2dc
virtual void OnReceiveWithdrawAlly(L2ParamStack&);                                                       // 184 +0x2e0
virtual void OnReceiveFriendAddRequestResult(L2ParamStack&);                                             // 185 +0x2e4
virtual void OnReceiveFriendAddRequest(L2ParamStack&);                                                   // 186 +0x2e8
virtual void OnReceiveFriendAdd(L2ParamStack&);                                                          // 187 +0x2ec
virtual void OnReceiveFriendRemove(L2ParamStack&);                                                       // 188 +0x2f0
virtual void OnReceiveFriendList(L2ParamStack&);                                                         // 189 +0x2f4
virtual void OnReceiveFriendStatus(L2ParamStack&);                                                       // 190 +0x2f8
virtual void OnReceiveBlockList(L2ParamStack&);                                                          // 191 +0x2fc
virtual void OnReceiveAskJoinParty(L2ParamStack&);                                                       // 192 +0x300
virtual void OnReceiveJoinParty(L2ParamStack&);                                                          // 193 +0x304
virtual void OnReceiveWithdrawalParty(L2ParamStack&);                                                    // 194 +0x308
virtual void OnReceiveOustPartyMember(L2ParamStack&);                                                    // 195 +0x30c
virtual void OnReceiveOustPartyMessage(L2ParamStack&);                                                   // 196 +0x310
virtual void OnReceiveDismissParty(L2ParamStack&);                                                       // 197 +0x314
virtual void OnRecevieDismissPartyMessage(L2ParamStack&);                                                // 198 +0x318
virtual void OnReceiveWareHouseDonePacket(L2ParamStack&);                                                // 199 +0x31c
virtual void OnReceiveShortCutList();                                                                    // 200 +0x320
virtual void AddShortCutItem(L2ParamStack&);                                                             // 201 +0x324
virtual void OnUpdateShortCutItem(INT);                                                                  // 202 +0x328
virtual void OnUpdatePetShortCutItem(INT);                                                               // 203 +0x32c
virtual void OnReceiveShortCutDelete(INT);                                                               // 204 +0x330
virtual void OnReceiveMagicSkillListPacket(L2ParamStack&);                                               // 205 +0x334
virtual void OnSay2(L2ParamStack&);                                                                      // 206 +0x338
virtual void OnReceiveVersionCheck(L2ParamStack&);                                                       // 207 +0x33c
virtual void OnReceiveAbnormalStatus();                                                                  // 208 +0x340
virtual void AddAbnormalStatus(L2ParamStack&);                                                           // 209 +0x344
virtual void OnReceiveShortStatus();                                                                     // 210 +0x348
virtual void AddShortStatus(L2ParamStack&);                                                              // 211 +0x34c
virtual void OnReceiveEtcStatus();                                                                       // 212 +0x350
virtual void AddEtcStatus(L2ParamStack&);                                                                // 213 +0x354
virtual void OnSetSSQStatus(L2ParamStack&);                                                              // 214 +0x358
virtual void OnSetSSQPreStatus(L2ParamStack&);                                                           // 215 +0x35c
virtual void OnStartSSQMainEvent();                                                                      // 216 +0x360
virtual void OnSetSSQMainEvent(L2ParamStack&);                                                           // 217 +0x364
virtual void OnSetSSQSealStatus(L2ParamStack&);                                                          // 218 +0x368
virtual void OnSSQInfo(ANSICHAR, ANSICHAR);                                                              // 219 +0x36c
virtual void OnAgitInfo(INT, const BYTE*);                                                               // 220 +0x370
virtual void OnPetitionVote();                                                                           // 221 +0x374
virtual void OnReceiveQuestList();                                                                       // 222 +0x378
virtual void OnEndQuestList();                                                                           // 223 +0x37c
virtual void AddQuestID(INT, INT);                                                                       // 224 +0x380
virtual void AddQuestItem(ItemInfo&);                                                                    // 225 +0x384
virtual void OnUpdateEquipItem(L2ParamStack&);                                                           // 226 +0x388
virtual void OnPartyAddMemberPacket(L2ParamStack&);                                                      // 227 +0x38c
virtual void OnPartyUpdateMemberPacket(L2ParamStack&);                                                   // 228 +0x390
virtual void OnPartyDeleteMemberPacket(L2ParamStack&);                                                   // 229 +0x394
virtual void OnPartyDeleteAllPacket(L2ParamStack&);                                                      // 230 +0x398
virtual void OnShowMagicWnd();                                                                           // 231 +0x39c
virtual void OnShowSkillWnd();                                                                           // 232 +0x3a0
virtual void OnSkillListPacket(L2ParamStack&);                                                           // 233 +0x3a4
virtual void OnMagicListPacket(L2ParamStack&);                                                           // 234 +0x3a8
virtual void OnReceiveTradeDemand(L2ParamStack&);                                                        // 235 +0x3ac
virtual void OnRestartResponse(L2ParamStack&);                                                           // 236 +0x3b0
virtual void OnAddSystemMessageParam(L2ParamStack&);                                                     // 237 +0x3b4
virtual void OnEndSystemMessageParam(L2ParamStack&);                                                     // 238 +0x3b8
virtual void OnSetupGaugePacket(L2ParamStack&);                                                          // 239 +0x3bc
virtual void OnShowboardPacket(bool, L2ParamStack&);                                                     // 240 +0x3c0
virtual void OnSelectItemToEnchant(INT);                                                                 // 241 +0x3c4
virtual void OnReceiveEnchantResult(INT);                                                                // 242 +0x3c8
virtual void OnReceiveAction(INT);                                                                       // 243 +0x3cc
virtual void OnShowTrainListWnd();                                                                       // 244 +0x3d0
virtual void OnShowTrainInfoWnd(L2ParamStack&);                                                          // 245 +0x3d4
virtual void AddAcquireSkillInfo(L2ParamStack&);                                                         // 246 +0x3d8
virtual void AddAcquireSkillExtendedInfo(L2ParamStack&);                                                 // 247 +0x3dc
virtual void OnShowQuestInfo(L2ParamStack&);                                                             // 248 +0x3e0
virtual void OnShowQuestMark(L2ParamStack&);                                                             // 249 +0x3e4
virtual void OnSendManorList(L2ParamStack*);                                                             // 250 +0x3e8
virtual void OnShowSeedInfo(L2ParamStack*);                                                              // 251 +0x3ec
virtual void OnShowCropInfo(L2ParamStack*);                                                              // 252 +0x3f0
virtual void OnShowManorDefaultInfo(L2ParamStack*);                                                      // 253 +0x3f4
virtual void OnShowSeedSetting(L2ParamStack*);                                                           // 254 +0x3f8
virtual void OnShowCropSetting(L2ParamStack*);                                                           // 255 +0x3fc
virtual void OnShowSellCropList(L2ParamStack*);                                                          // 256 +0x400
virtual void OnShowProcureCropDetail(L2ParamStack*);                                                     // 257 +0x404
virtual void OnHeroList(L2ParamStack*);                                                                  // 258 +0x408
virtual void OnOlympiadUserInfo(L2ParamStack&);                                                          // 259 +0x40c
virtual void OnOlympiadSpelledInfo(L2ParamStack&);                                                       // 260 +0x410
virtual void OnOlympiadMode(ANSICHAR);                                                                   // 261 +0x414
virtual void OnOlympiadMatchEnd();                                                                       // 262 +0x418
virtual void OnExMailArrivedPacket();                                                                    // 263 +0x41c
virtual void OnExStorageMaxCountPacket(L2ParamStack&);                                                   // 264 +0x420
virtual void OnExOpenMPCC(L2ParamStack&);                                                                // 265 +0x424
virtual void OnExCloseMPCC(L2ParamStack&);                                                               // 266 +0x428
virtual void OnExAskJoinMPCC(L2ParamStack&);                                                             // 267 +0x42c
virtual void OnHideTrainWnd();                                                                           // 268 +0x430
virtual void OnReceiveRecipeBookItemList(INT, INT);                                                      // 269 +0x434
virtual void AddRecipeBookItem(INT);                                                                     // 270 +0x438
virtual void ReceiveRecipeItemMakeInfo(L2ParamStack&);                                                   // 271 +0x43c
virtual void ReceiveRecipeShopManageList(INT, INT, INT);                                                 // 272 +0x440
virtual void AddRecipeShopManageItem(INT, INT);                                                          // 273 +0x444
virtual void AddRecipeShopRegisterdItem(INT, INT, INT);                                                  // 274 +0x448
virtual void ReceiveRecipeShopSellList(INT, INT, INT, INT);                                              // 275 +0x44c
virtual void AddRecipeShopSellItem(INT, INT, INT);                                                       // 276 +0x450
virtual void ReceiveRecipeShopItemInfo(INT, INT, INT, INT, INT, INT);                                    // 277 +0x454
virtual void RequestRecipeBookOpen(L2ParamStack&);                                                       // 278 +0x458
virtual void RequestRecipeItemDelete(INT);                                                               // 279 +0x45c
virtual void RequestRecipeItemMakeInfo(INT);                                                             // 280 +0x460
virtual void RequestRecipeItemMakeSelf(INT);                                                             // 281 +0x464
virtual void RequestRecipeShopManageList();                                                              // 282 +0x468
virtual void RequestRecipeShopMessageSet(TCHAR*);                                                        // 283 +0x46c
virtual void RequestRecipeShopListSet(L2ParamStack&);                                                    // 284 +0x470
virtual void RequestRecipeShopManageQuit();                                                              // 285 +0x474
virtual void RequestRecipeShopManageCancel();                                                            // 286 +0x478
virtual void RequestRecipeShopSellList(INT);                                                             // 287 +0x47c
virtual void RequestRecipeShopMakeInfo(INT, INT);                                                        // 288 +0x480
virtual void RequestRecipeShopMakeDo(INT, INT, INT);                                                     // 289 +0x484
virtual void ReceiveMacroListInfo(ANSICHAR, ANSICHAR, MacroInfo, ANSICHAR, INT);                         // 290 +0x488
virtual void ReceiveHennaEquipList(INT, INT);                                                            // 291 +0x48c
virtual void AddHennaEquipInfo(HennaInfo);                                                               // 292 +0x490
virtual void ReceiveHennaItemInfo(HennaInfo, INT);                                                       // 293 +0x494
virtual void ReceiveHennaInfo(HennaInfo, INT);                                                           // 294 +0x498
virtual void AddHennaInfo(INT, INT);                                                                     // 295 +0x49c
virtual void ReceiveHennaUnequipList(INT, INT);                                                          // 296 +0x4a0
virtual void AddHennaUnequipInfo(HennaInfo);                                                             // 297 +0x4a4
virtual void ReceiveHennaUnequipItemInfo(HennaInfo, INT);                                                // 298 +0x4a8
virtual void RequestHennaItemInfo(INT);                                                                  // 299 +0x4ac
virtual void RequestHennaEquip(INT);                                                                     // 300 +0x4b0
virtual void RequestHennaUnequipInfo(INT);                                                               // 301 +0x4b4
virtual void RequestHennaUnequip(INT);                                                                   // 302 +0x4b8
virtual void RequestHennaItemList();                                                                     // 303 +0x4bc
virtual void RequestHennaUnequipList();                                                                  // 304 +0x4c0
virtual void GMAddHennaInfo(INT, INT);                                                                   // 305 +0x4c4
virtual void GMReceiveHennaInfo(HennaInfo, INT);                                                         // 306 +0x4c8
virtual void EnablePartyChat(bool);                                                                      // 307 +0x4cc
virtual void EnablePledgeChat(bool);                                                                     // 308 +0x4d0
virtual void EnableAllyChat(bool);                                                                       // 309 +0x4d4
virtual void OnShowPetStatusWnd(INT);                                                                    // 310 +0x4d8
virtual void OnShowPetPartyWnd(INT);                                                                     // 311 +0x4dc
virtual void OnHidePetWnd(INT);                                                                          // 312 +0x4e0
virtual void OnShowPetNameBtn(INT);                                                                      // 313 +0x4e4
virtual void OnReceivePetStatusInfo(INT, L2ParamStack&);                                                 // 314 +0x4e8
virtual void OnReceivePetItemList();                                                                     // 315 +0x4ec
virtual void AddPetInventoryItem(ItemInfo&);                                                             // 316 +0x4f0
virtual void UpdatePetInventoryItem(INT, ItemInfo&);                                                     // 317 +0x4f4
virtual void OnGMViewCharacterInfo(User&);                                                               // 318 +0x4f8
virtual void OnGMViewReceivePledgeInfo(PledgeInfo&);                                                     // 319 +0x4fc
virtual void OnGMViewReceivePledgeMemberList();                                                          // 320 +0x500
virtual void OnGMViewAddPledgeMember(L2ParamStack&);                                                     // 321 +0x504
virtual void OnGMViewShowSkillWnd();                                                                     // 322 +0x508
virtual void OnGMViewSkillListPacket(L2ParamStack&);                                                     // 323 +0x50c
virtual void OnGMViewShowMagicWnd();                                                                     // 324 +0x510
virtual void OnGMViewMagicListPacket(L2ParamStack&);                                                     // 325 +0x514
virtual void OnGMViewReceiveQuestList();                                                                 // 326 +0x518
virtual void GMViewAddQuestID(INT, INT);                                                                 // 327 +0x51c
virtual void GMViewAddQuestItem(ItemInfo&);                                                              // 328 +0x520
virtual void OnGMViewReceiveItemList(INT, INT);                                                          // 329 +0x524
virtual void GMViewAddInventoryItem(ItemInfo&);                                                          // 330 +0x528
virtual void OnGMViewStartWithdrawList(INT);                                                             // 331 +0x52c
virtual void OnGMViewAddWithdrawList(ItemInfo&);                                                         // 332 +0x530
virtual void AddPackageToList(L2ParamStack&);                                                            // 333 +0x534
virtual void AddSendableItemList(INT, INT, INT, ItemInfo&);                                              // 334 +0x538
virtual void OnShowMinimap(INT);                                                                         // 335 +0x53c
virtual void OnShowCalc(INT);                                                                            // 336 +0x540
virtual void OnStartPrivateStoreManageList(L2ParamStack&);                                               // 337 +0x544
virtual void OnStartPrivateStoreList(L2ParamStack&);                                                     // 338 +0x548
virtual void OnPrivateStoreManageList(ItemInfo&, ItemInfo&);                                             // 339 +0x54c
virtual void OnPrivateStoreList(ItemInfo&, INT, INT);                                                    // 340 +0x550
virtual void OnPrivateStoreMsg(TCHAR*, INT);                                                             // 341 +0x554
virtual void OnStartPrivateStoreBuyManageList();                                                         // 342 +0x558
virtual void OnStartPrivateStoreBuyList();                                                               // 343 +0x55c
virtual void OnPrivateStoreBuyManageList(ItemInfo&, ItemInfo&, INT, INT);                                // 344 +0x560
virtual void OnPrivateStoreBuyList(ItemInfo&, INT, INT);                                                 // 345 +0x564
virtual void OnPrivateStoreBuyMsg(TCHAR*, INT);                                                          // 346 +0x568
virtual void OnPlaySound(L2ParamStack&);                                                                 // 347 +0x56c
virtual void OnInterfacePlaySound(TCHAR*);                                                               // 348 +0x570
virtual void OnInterfacePlayVoice(TCHAR*);                                                               // 349 +0x574
virtual INT OnStaticObjectInfo(L2ParamStack&, AActor**);                                                 // 350 +0x578
virtual void OnDeleteStaticObject(FServerStaticObject*);                                                 // 351 +0x57c
virtual INT OnMoveToActor(User*, AActor*, INT, FVector);                                                 // 352 +0x580
virtual void OnReceiveMagicSkillUse(User*, User*, L2ParamStack&);                                        // 354 +0x588
virtual void OnReceiveMagicSkillUse(User*, INT, L2ParamStack&);                                          // 353 +0x584
virtual void OnReceiveMagicSkillCanceled(User*);                                                         // 355 +0x58c
virtual INT OnReceiveMagicSkillLaunched(User*, User*, INT, INT, INT);                                    // 357 +0x594
virtual INT OnReceiveMagicSkillLaunched(User*, INT, INT, INT, INT);                                      // 356 +0x590
virtual INT OnAssociatedAttack(User*, User*, INT, INT, INT, INT, INT, FVector, INT);                     // 358 +0x598
virtual INT OnClientSetTime(L2ParamStack&);                                                              // 359 +0x59c
virtual void OnVehicleStartPacket(L2ParamStack&);                                                        // 360 +0x5a0
virtual void OnRequestTimeCheckPacket(L2ParamStack&);                                                    // 361 +0x5a4
virtual void OnReceiveServerTime(INT);                                                                   // 362 +0x5a8
virtual void OnSkillCoolTimePacket(L2ParamStack&);                                                       // 363 +0x5ac
virtual void OnStartAllianceWar(L2ParamStack&);                                                          // 364 +0x5b0
virtual void OnStopAllianceWar(L2ParamStack&);                                                           // 365 +0x5b4
virtual void OnReceiveSiegeInfo(L2ParamStack&);                                                          // 366 +0x5b8
virtual void OnReceiveSiegeAttackerList(L2ParamStack&);                                                  // 367 +0x5bc
virtual void OnReceiveSiegeDefenderList(L2ParamStack&);                                                  // 368 +0x5c0
virtual void OnAddSelectableTime(INT);                                                                   // 369 +0x5c4
virtual void OnEndSiegeAttackerlist();                                                                   // 370 +0x5c8
virtual void OnEndSiegeDefenderlist();                                                                   // 371 +0x5cc
virtual void ClearAttackerList();                                                                        // 372 +0x5d0
virtual void ClearDefenderList();                                                                        // 373 +0x5d4
virtual void OnSetVIPShopID(INT);                                                                        // 374 +0x5d8
virtual void OnAddVIPItemList(INT, INT, INT, INT, INT, INT, INT, INT);                                   // 375 +0x5dc
virtual void OnAddVIPNeedItemList(INT, INT, INT, INT);                                                   // 376 +0x5e0
virtual void OnVIPItemListEnd();                                                                         // 377 +0x5e4
virtual void OnSetSummonRemainTime(INT, INT);                                                            // 378 +0x5e8
virtual void SetServerPing(INT, DWORD);                                                                  // 379 +0x5ec
virtual void OnEndItemList();                                                                            // 380 +0x5f0
virtual void OnSnoop(L2ParamStack&);                                                                     // 381 +0x5f4
virtual void OnShowTownMapPacket(L2ParamStack&);                                                         // 382 +0x5f8
virtual void OnObserverStartPacket(L2ParamStack&);                                                       // 383 +0x5fc
virtual void OnObserverEndPacket(L2ParamStack&);                                                         // 384 +0x600
virtual void OnChairSitPacket(L2ParamStack&);                                                            // 385 +0x604
virtual void OnPledgePower(INT, INT, BYTE*);                                                             // 386 +0x608
virtual INT OnConfirmDlg(L2ParamStack&);                                                                 // 387 +0x60c
virtual void OnPartySpelledPacket(L2ParamStack&);                                                        // 388 +0x610
virtual void OnCameraMode(INT);                                                                          // 389 +0x614
virtual void OnShowXMasSeal(INT);                                                                        // 390 +0x618
virtual void OnGameGuardQuery(DWORD, DWORD, DWORD, DWORD, INT);                                          // 391 +0x61c
virtual void OnReceiveL2FriendList();                                                                    // 392 +0x620
virtual void OnAddL2Friend(INT, TCHAR*, INT, INT);                                                       // 393 +0x624
virtual void OnEndL2FriendList();                                                                        // 394 +0x628
virtual void OnUpdateL2Friend(INT, INT, TCHAR*, INT, INT);                                               // 395 +0x62c
virtual void OnUpdateL2FriendStatus(INT, TCHAR*, INT);                                                   // 396 +0x630
virtual void OnReceiveL2FriendSay(INT, TCHAR*, TCHAR*);                                                  // 397 +0x634
virtual void OnRegenStatus(INT, INT, INT, DOUBLE);                                                       // 398 +0x638
virtual void OnEventMatchObserver(INT, ANSICHAR, ANSICHAR, FString, FString, INT, INT, INT);             // 399 +0x63c
virtual void OnEventMatchMessage(ANSICHAR, FString);                                                     // 400 +0x640
virtual void OnEventMatchScore(INT, INT, INT);                                                           // 401 +0x644
virtual void OnEventMatchTeamInfoStart(INT, ANSICHAR, INT);                                              // 402 +0x648
virtual void OnEventMatchTeamInfo(INT, INT, FString, INT, INT, INT, INT, INT, INT, INT, INT, INT, INT);  // 403 +0x64c
virtual void OnEventMatchTeamInfoEnd();                                                                  // 404 +0x650
virtual void OnEventMatchCreate(INT);                                                                    // 405 +0x654
virtual void OnEventMatchUserInfo(INT, FString, INT, INT, INT, INT, INT, INT, INT, INT);                 // 406 +0x658
virtual void OnEventMatchSpellInfoStart(INT, INT, INT);                                                  // 407 +0x65c
virtual void OnEventMatchSpellInfo(INT, INT, INT, INT);                                                  // 408 +0x660
virtual void OnEventMatchSpellInfoEnd();                                                                 // 409 +0x664
virtual void OnFirecracker(APawn*, INT);                                                                 // 410 +0x668
virtual void OnEventMatchTeamUnlocked(INT, ANSICHAR);                                                    // 411 +0x66c
virtual void OnEventMatchGMTest();                                                                       // 412 +0x670
virtual void OnColosseumFence(ColosseumFence*, ColosseumFenceState);                                     // 413 +0x674
virtual void OnListPartyWating(L2ParamStack&);                                                           // 414 +0x678
virtual void OnExPartyRoomMember(L2ParamStack&);                                                         // 415 +0x67c
virtual void OnExClosePartyRoom(L2ParamStack&);                                                          // 416 +0x680
virtual void OnPartyRoomInfo(L2ParamStack&);                                                             // 417 +0x684
virtual void OnExManagePartyRoomMember(L2ParamStack&);                                                   // 418 +0x688
virtual void OnStartPartyMatch();                                                                        // 419 +0x68c
virtual void OnExAutoSoulShot(L2ParamStack&);                                                            // 420 +0x690
virtual void OnExEventMatchManage(L2ParamStack&);                                                        // 421 +0x694
virtual void OnExEventMatchManageAddMember(L2ParamStack&);                                               // 422 +0x698
virtual void OnFishingStart(APawn*, INT, FVector);                                                       // 423 +0x69c
virtual void OnFishingEnd(APawn*, BYTE);                                                                 // 424 +0x6a0
virtual void OnFishingCombatStart(APawn*, L2ParamStack&);                                                // 425 +0x6a4
virtual void OnFishingHPRegen(APawn*, L2ParamStack&);                                                    // 426 +0x6a8
virtual void OnExMultiPartyCommandChannelInfo(L2ParamStack&);                                            // 427 +0x6ac
virtual void OnExMultiPartyCommandChannelInfoAddMember(L2ParamStack&);                                   // 428 +0x6b0
virtual void OnExPCCafePointInfo(L2ParamStack&);                                                         // 429 +0x6b4
