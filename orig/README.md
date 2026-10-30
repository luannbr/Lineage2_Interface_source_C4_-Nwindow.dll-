# Binários de referência (fora do git) / Reference binaries (not in git)

**PT.** Esta pasta não vem com nenhum arquivo do jogo: nenhum binário da NCSoft é publicado. Copie os
arquivos abaixo da pasta `system` do **seu próprio** cliente Lineage II C4 (Scions of Destiny) para cá. O
build do modo hook (`NWindowDecomp`) não precisa deles (as bibliotecas de import saem de
`third_party/ue_import/*.def`); o `NWindowStandalone` precisa de `orig/NWindow.dll` (os cursores coloridos são
extraídos dele na compilação); as ferramentas de verificação e análise precisam do `NWindow.dll`, do
`Core.dll` e do `Engine.dll`. O `NWindow.dll` tem de ter o md5 da tabela.

**EN.** This folder ships without any game file: no NCSoft binary is published. Copy the files below from
the `system` folder of **your own** Lineage II C4 (Scions of Destiny) client into it. The hook-mode build
(`NWindowDecomp`) does not need them (its import libraries come from `third_party/ue_import/*.def`);
`NWindowStandalone` needs `orig/NWindow.dll` (the colour cursors are extracted from it at build time); the
verification and analysis tools need `NWindow.dll`, `Core.dll` and `Engine.dll`. The `NWindow.dll` must have
the md5 in the table.

| Arquivo / File | md5 | TimeDateStamp (PE) | Uso / Use |
|---|---|---|---|
| `NWindow.dll` | `afab3ae3fa4f85f7f4c323715e5155e3` | `0x43C73652` (13/01/2006) | **o** binário que o projeto reconstrói / **the** binary this project rebuilds (3,112,980 bytes, `SizeOfCode` `0x19C000`, entry `0x10172B4A`). Every tool checks its md5 (`tools/nwd/image.py: STOCK_MD5`) |
| `Core.dll` | `34b0218ec506a3bc5940ced00d8db5e7` | `0x4477B8A7` (27/05/2006) | declarations in `include/ue2`, `third_party/ue_import/Core.def` |
| `Engine.dll` | `db71fb8d851b65c7e159855cb6d23796` | `0x4477B930` (27/05/2006) | `Engine.def`; over 8,192 exports (`tools/engine_vtables.py`, `tools/gen_import_defs.py`) |
| `npkcrypt.dll` | `eda437df20e1618d8316f0142c9b071d` | `0x437194B0` (09/11/2005) | nProtect KeyCrypt: the stock `NWindow.dll` imports 8 functions from it by ordinal (the standalone loads it with `LoadLibrary`) |
| `WinDrv.dll` | `48550ef13b6c28384927281a88de2ddc` | `0x4477AA92` (27/05/2006) | reference only (`UViewport` virtuals) |
| `Window.dll` | `853fbe840cc7784e89094c95e6319276` | `0x4477AA0B` (27/05/2006) | reference only |
| `ALAudio.dll` | `c306a674cc323bbf7c3e123f3906eb89` | `0x4477AACE` (27/05/2006) | `UALAudioSubsystem`: names the pure virtuals of `UAudioSubsystem` (`tools/gen_engine_decls.py`) |
| `D3DDrv.dll` | `0de570a002dd803d9776d30a76dd4c14` | `0x4477AA37` (27/05/2006) | `UD3DRenderDevice`: the pure virtuals of `URenderDevice` |
| `IpDrv.dll`, `Fire.dll`, `LineageEnv.dll` | `73286ba8…`, `eac32312…`, `e4ac5f6b…` | 27/05/2006 | reference only |
| `l2.exe` | `ee6d26494c7e243d1f0841a48032e20a` | `0x4394DC82` (06/12/2005) | reference only |

**Atenção / Note:** o `NWindow.dll` é de 13/01/2006, mas o `Engine.dll`/`Core.dll` ao lado são de 27/05/2006
(um pacote de cliente atualizado); o projeto honra esse conjunto, que é o que roda. The C4 client has no
`Network.dll`: the network code is in `Engine.dll`/`IpDrv.dll`.

Para recriar a pasta / to fill the folder (`cmd`, from the repository root; `<C4 client>` = your client):

```bat
set C4=<C4 client>\system
for %f in (NWindow Core Engine npkcrypt WinDrv Window ALAudio D3DDrv IpDrv Fire LineageEnv) do copy "%C4%\%f.dll" orig\
copy "%C4%\L2.exe" orig\l2.exe
```

A pasta de teste (para `tools/install_test.py`, `tools/cycle.py`, `tools/smoke_standalone.py`) é uma **cópia**
da `system` inteira; a `system` original nunca é tocada. The test folder is a **copy** of the whole `system`
folder; point `NWD_CLIENT_SYSTEM` (or `--system`) at it:

```bat
xcopy /E /I /Y "<C4 client>\system" "<C4 client>\system_nwtest"
set NWD_CLIENT_SYSTEM=<C4 client>\system_nwtest
```
