# gb_emu
Gameboy Emulator written in C

ゲームボーイのエミュレータ。

以下のソフトが動きました：
* ポケットモンスター 赤
* ポケットモンスター 銀
* 星のカービィ2
* STREET FIGHTER II
* SUPER MARIO LAND
* スーパーマリオランド2 6つの金貨
* ドラゴンクエスト I, II

# Build
```
make
```
Depends: libsdl2

# Usage
```
./gb_emu ROMfile [-s SaveData(Cartridge RAM)] [-z Zoom] [-d force DMG(monochrome) mode]
```
