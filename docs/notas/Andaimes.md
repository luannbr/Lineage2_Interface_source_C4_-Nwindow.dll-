> **Nota do C2 (06/10/2026):** este documento é do projeto do C1 e descreve o `NWindow.dll` do C1 (178 classes, endereços `0x100…` do C1). Os fatos do C2 e o que já foi retargetado estão em [../PLANO_C2.md](../PLANO_C2.md); os endereços daqui não valem para a imagem do C2 (o `db/fmap.csv` traduz C1 → C2).

# Andaimes do modo gancho — o que sobra e por quê

Levantado em 26/09/2026 (rodada 9 do trabalhador menus) e refeito em 27/09/2026 (rodada 10,
fase 5, passo 2 do `docs/notas/Standalone.md`, seção 9) sobre o master `eb11e6a` mais os
commits da rodada. Complementa o `Standalone.md`: lá estão as contas gerais (`NW_HOOK_*`,
`NW_CHECK_VTABLE`, `NClass::X()`), as globais que moram no original (seção 5), a tabela de quem
bloqueia o link (seção 6) e o CRT (seção 7); aqui fica a lista item a item do que ainda é
andaime no código das janelas, o motivo e o que destrava cada um.

**Contagem nesta branch** (fora de comentários):

| Andaime | Rodada 9 | Agora | Fora de `#if NW_HOOK_MODE` |
|---|---|---|---|
| `NW_STOCK` | 5 | 5 | **0** |
| `nw::StockCall` | 4 → 3 | 3 | **0** |
| `nw::Field` | 35 → 5 | **0** | 0 |
| `nw::VirtualCall` | 196 (29 fora do console) | **1** | 1 (não bloqueia o link) |

Ou seja, no build standalone nada do código das janelas chama o original; o que sobra de
`NW_STOCK`/`StockCall` só existe no ramo do modo gancho, de propósito (seção 2).

## 1. O que saiu na rodada 10

### Chamadas ao original (`NStdLib.*`, `NStl.cpp`)

As exceções da biblioteca padrão ganharam os dois ramos:

- **Modo gancho, igual a antes.** Os objetos usam as vtables do original
  (`GStdLogicErrorVtbl`, `GStdLengthErrorVtbl`) e são lançados com o `ThrowInfo` dele
  (`GStdLengthErrorThrowInfo`) pelo `_CxxThrowException` do CRT velho. O construtor, a cópia
  e o destrutor do `std::exception` e o `_Xlen`/`_Xran` do `<xstring>` continuam sendo os do
  original. Tudo isso agora fica dentro de `#if NW_HOOK_MODE`, inclusive os três `nw::Addr`.
- **Standalone.**
  - O `std::exception` é nosso, escrito como o `exception.cpp` do VC 7.0: a cópia duplica a
    mensagem com `malloc`, o destrutor a libera, o `what()` devolve "Unknown exception" sem
    mensagem.
  - As classes têm virtuais de verdade (slot 0 o destrutor de apagar, slot 1 o `what()`, como
    as vtables do original).
  - O `length_error` e o novo `NStdOutOfRange` saem por `throw` (o `ThrowInfo` é o do nosso
    compilador).
  - O `_Xlen`/`_Xran` viraram `NStdStringXlen`/`NStdStringXran`, usados pelo `std::string`
    (`NStdString`) e pelo `std::wstring` (`NStlWString`), como no original.

Conferido fora do build do projeto (ainda não existe alvo standalone):

- o `NStdLib.cpp` e o `NStl.cpp` compilam com `NW_HOOK_MODE=0` (um cabeçalho provisório com as
  macros do rascunho do lobby);
- os objetos só referenciam o CRT novo, o `GMalloc` e o que o compilador gera;
- um teste de rascunho lançou e pegou as exceções: os `catch` por tipo batem, o `what()` sai
  pela vtable, a cópia por valor funciona e nenhum bloco do `GMalloc` fica para trás.

### `nw::Field`: os últimos cinco

- `USkeletalMesh::DefaultAnim` (+0x23C), `UMeshAnimation::AnimSeqs` (+0x44, sequências de 0x40
  bytes com o nome primeiro) e `USkeletalMeshInstance::Blends` (+0x114, canais de 0x70 bytes:
  `AnimRate` +0xC e a taxa base +0x20).
- Os visualizadores leem esses campos; saíram `MeshAnimation`, `AnimSeqCount` e `AnimSeqName`
  do `NViewerData.h`.

### `nw::VirtualCall`: de 196 para 1

- **Listas geradas.** O `tools/gen_engine_decls.py` agora escreve as virtuais de 12 classes do
  Engine (`include/ue2/gen/*.vt.inc`):
  - `UPrimitive`, `UMesh`, `AActor`, `APawn`, `AController`;
  - `UPlayer`, `UViewport`;
  - `UAudioSubsystem`, `URenderDevice`, `UEngine`, `UGameEngine`;
  - `UL2NetHandler`.
- **As virtuais puras ganham nome pela implementação exportada.** Os DLLs do cliente exportam
  as classes concretas com os nomes decorados, então nome e assinatura são exatos:
  - o `UNetworkHandler` do `Network.dll` dá os 203 slots do `UL2NetHandler`;
  - o `UALAudioSubsystem` (`ALAudio.dll`), o `UD3DRenderDevice` (`D3DDrv.dll`) e o
    `UWindowsViewport` (`WinDrv.dll`) dão os do áudio, do renderizador e da viewport;
  - o `UGameEngine` dá os do `UEngine`.
- **Conferência.** O relatório de layout do MSVC (`/d1reportSingleClassLayout`) das 12 classes
  bate slot a slot com as vtables do original, e o verificador confere cada chamada. O
  gerador recusa um nome repetido em dois trechos separados de slots (o MSVC agruparia as
  sobrecargas).
- **Nomes do Engine que vieram junto:**
  - o `FL2PetInfo` é o `PetInfo`;
  - o `FL2PledgeInfo` era outra visão do `PledgeInfo` e foi fundido nele (nome da aliança e
    texturas dos brasões);
  - o `NCharacterCreateInfo` é o `CharacterInfo` (foi para o `UE2Engine.h`, ao lado do
    `ItemInfo`);
  - `Item::ObjectID`, `FBox` e `FRenderCaps`;
  - o slot 145 do `AController`, sem nome na rodada 9, é o `StopMove`.
- **Embrulhos que sumiram.** Os que só existiam porque as virtuais não estavam declaradas,
  vários com nomes inventados:
  - `ActorSetPhysics`, `ActorPlayAnim` e `MeshGetInstance` (`NViewerData.h`);
  - os quatro do volume (`NCAudioOptionWnd.cpp`);
  - os sete do renderizador (`NCVideoOptionWnd.cpp`): o `GetDisplayModes` era o `GetRes`, o
    `ApplyTextureDetail` era o `DeleteAllTexture` e o `GetTrilinear` era o `IsUseTrilinear`.
- **O timer do console** chama `static_cast<NCWnd*>(Timer.Owner)->WindowProc(...)`.

Os tipos dos argumentos foram conferidos contra as assinaturas: compilando os arquivos em `/W4`
não há conversão nos pontos trocados.

Duas notas do verificador mudaram:

- `RequestDropItem` não precisa mais da dela: a chamada tipada fica em linha;
- `RequestAttack` ganhou uma: o compilador carrega o slot de cada ramo (Attack/Action) e faz
  uma chamada só, e o verificador só vê o último.

### Funções mantidas para arquivos de outros trabalhadores (seção 5 da rodada 9)

Saíram todas:

- `NpcTable`, `ControllerPawn` e `UserEquippedObjectID`;
- `NItemTable::IconName/Name/ArmorType`;
- a cópia `NItemData` do `NConsoleWndItems.cpp` (agora usa o `NViewItemData`).

## 2. O que sobra e por quê

| Onde | O quê | Por que fica |
|---|---|---|
| `NConsoleWndNet.cpp` `vfn176` | `nw::VirtualCall` do slot 174 do `UL2NetHandler` | O original salta para o slot (`jmp [edx+0x2b8]`) com o `L2ParamStack&` ainda na pilha: os cabeçalhos com que ele foi compilado davam um argumento a esse slot, mas o `RequestSkillCoolTime` do `Network.dll` não recebe nenhum (nem o tira da pilha). Chamar pela declaração mudaria o que chega na pilha; fica pelo slot, anotado. Nenhum código do NWindow chama o `vfn176`. Não bloqueia o link: o `NwStandalone.h` do lobby mantém o `nw::VirtualCall`. |
| `NStdLib.cpp` | `NW_STOCK`: `_CxxThrowException`, `_Xlen`, `_Xran` | Só no modo gancho: a exceção atravessa quadros do original, que a pegam e destroem pelo `ThrowInfo` e pelas vtables dele. |
| `NStl.cpp` | `NW_STOCK`: `NStlWString::Xran/Xlen` | Idem. |
| `NStdLib.h` | `nw::StockCall` do construtor, da cópia e do destrutor do `std::exception`; `nw::Addr` das duas vtables e do `ThrowInfo` | Idem; o destrutor libera com o `free` do CRT velho o que a cópia do CRT velho alocou. |

**Fora do escopo dos andaimes** (anotado para não confundir quem procura):

- **Os deslocamentos crus do `NCChineseIME.cpp` e do `NCTaiwanIME.cpp`.** Leem a área privada
  (`IMCC`) dos IMEs da Microsoft, uma estrutura sem documentação. O original lê do mesmo
  jeito; não há o que declarar do nosso lado.
- **As globais.** `NW_GLOBAL`, os `nw::Addr` das tabelas e o `NClasses.gen.h` são do status e
  do lobby (`Standalone.md`, seções 5 e 9).
- **Os ganchos que só existem por causa do modo gancho** (rodada 9, seção 6):
  - o `NewChineseIME`/`NewTaiwanIME`;
  - as instâncias do `NTemplates.cpp`.

  Continuam iguais: existem para o `NW_HOOK_FUNC`; no standalone o compilador gera as dele e
  os embrulhos sobram.

## 3. Ferramentas

- **Para regenerar os `include/ue2/gen/*.vt.inc`,** o `orig/` precisa do `Network.dll`, do
  `ALAudio.dll`, do `D3DDrv.dll` e do `WinDrv.dll` do cliente (md5 e cópia em
  `orig/README.md`).
- **O `tools/engine_vtables.py`** também procura classes no `Network.dll` quando ele está lá.
- **Para conferir uma lista gerada contra o original,** compilar um arquivo que inclua o
  `NWindow.h` com `/d1reportSingleClassLayout<Classe>` e comparar os nomes dos slots com o
  `tools/engine_vtables.py <Classe>`. Foi o que se fez nesta rodada; ainda não é ferramenta
  do projeto.
