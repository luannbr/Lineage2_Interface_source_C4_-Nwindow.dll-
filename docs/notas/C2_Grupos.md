# C2: grupos da onda 1 (porte à mão)

Cada grupo é uma lista de arquivos `src/nwindow/<Arquivo>.cpp` (e o `include/nwindow/<Arquivo>.h` de mesmo nome, quando existe)
que um agente pode editar. Um cabeçalho de classe-base que pertence a outro grupo só muda se o grupo dono mudar.
Fundação já fundida: NCGDevice, NCObject, NCWnd/NControl/NCCmdTarget, NCButton, NCCheckBox, NCFrameCtrl, NCIcon, NCScrollWnd, NCCompositeBox,
estruturas do Engine (User, FL2GameData, NItemInfo/NItemEntry, NDialogInfo...) e a vtable do NConsoleWnd (335 slots).

## console

NCConsole, NConsoleWnd, NConsoleWndActions, NConsoleWndData, NConsoleWndEvents, NConsoleWndInput, NConsoleWndItems, NConsoleWndLogin, NConsoleWndNet, NConsoleWndText, NConsoleWndWindows

## edit

NCCandidateBox, NCChineseIME, NCComboBox, NCCountSpin, NCDialogBox, NCEditBox, NCHtmlButton, NCHtmlComboBox, NCHtmlEdit, NCHtmlViewer, NCIME, NCIMEManager, NCJapaneseIME, NCKoreanIME, NCListBox, NCListCtrl, NCMessageWnd, NCMultiEdit, NCNPCHtmlViewer, NCTabControl, NCTaiwanIME, NCTutorialViewerWnd, NCWarningWnd, NHtmlLayout, NHtmlParser

## items

NCActionItemListWnd, NCActionItemWnd, NCActionLinker, NCActionWnd, NCCoolTimeIcon, NCDeliverWnd, NCEquipItemWnd, NCInvenItemWnd, NCInventoryWnd, NCItemWnd, NCMagicSkillWnd, NCPetInventoryWnd, NCPrivateBuyWnd, NCPrivateShopWnd, NCShopWnd, NCSkillItemWnd, NCSkillWnd, NCStoreWnd, NCTradeWnd, NCVIPShopItemListBox, NCVIPShopWnd

## pawn

NCAuthWnd, NCLobbyWnd, NCLoginWnd, NCMSViewrWnd, NCMinimapWnd, NCNpcViewrWnd, NCOptionWnd, NCPawnCreateWnd, NCPawnSelectWnd, NCPawnView_CharacterSelectWnd, NCPawnViewrWnd, NCPawnViewr_HairChangeWnd, NCPawnViewr_MixAnimWnd, NCPawnViewr_Simulator, NCRestartMenuWnd, NCServerSelectWnd, NCSystemMenuWnd

## status

NCAbnormalStatusWnd, NCBoardPage, NCBoardWnd, NCDetailStatusWnd, NCGMWnd, NCMatchListCtrl, NCMatchOptionWnd, NCMatchWnd, NCPartyWnd, NCPetWnd, NCPetitionListBox, NCPetitionWnd, NCPledgeWnd, NCQuestInfoWnd, NCQuestItemWnd, NCQuestWnd, NCSiegeInfoMainWnd, NCStatusWnd, NCTrainListBox, NCTrainWnd

## chatopt

NCAudioOptionWnd, NCAutoLogWnd, NCChatFilterWnd, NCChatListBox, NCChatWnd, NCDebugWnd, NCGameOptionWnd, NCShortCutWnd, NCVideoOptionWnd, NCMainWnd, NCMenuWnd, NCNoticeWnd, NCRadarWnd, NCTestHtmlViewer, NCVirtualWndMain, NCommandParser, NL2Font, NStdLib
