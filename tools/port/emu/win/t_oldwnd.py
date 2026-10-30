# The C4 successor of the C2-era window tests t_macro.py, t_recipe.py, t_shortcut.py, t_gm.py and t_equippaint.py (tools/port/emu): the same functions
# (macro windows, recipe making windows, shortcut bar, GM windows, paperdoll), at their C4 addresses, run stock against ours with the generic
# world of t_sys.py (fake windows made from the headers' `// +0xNN` fields, every callee a logging stub identified by its stock address, the
# device as a fake object) instead of the C2 hand-made models. A row is `T(label, class, stock va, argument kind, void)`; the argument kinds
# are t_sys.py's (paint = a device, int1/int2 = integers from the function's own constants, mouse, key, none ...).
#   python t_oldwnd.py [cases] [seed] [name-part ...]        (no names: every row of OLD; NWD_SA=1 for the standalone; OLDWND_ALL=1 adds the GAPS rows)
# After each row t_sys prints the stock instructions the cases reached (`coverage`): the random fill reaches only part of the bigger painters
# (e.g. NCMacroArrayWnd::OnPaint 60 of 290, NCShortCutWnd::OnLButtonUp 47 of 453, NCGMEquipItemWnd::OnPaint 58 of 629 at 20 cases); read it before
# trusting a CLEAN. A 20-case run takes about 6 minutes (the world is built per class); clean at 20 cases, seed 1, in both modes.
import sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import t_sys
from t_sys import *

t_sys.TESTS[:] = []          # (importing t_sys registered its own table)
T = t_sys.T

# the CRT's float-to-int helper asks IsProcessorFeaturePresent in ours (t_itemwnd.py / e6.py answer it the same way): not a service of the test
_dispatch = t_sys.SYS.dispatch


def dispatch(self, name):
    if name == 'imp:IsProcessorFeaturePresent':
        self.last_pop = 4
        return 0
    return _dispatch(self, name)


t_sys.SYS.dispatch = dispatch
# Shared::GetFirstDword_ICF (0x1016EC20: `mov eax, [ecx]; ret`, a folded getter): ours inlines it, so the stock runs it for real as well
FOLDED = (0x1016EC20,)

OLD = [
    # t_macro.py: NCMacroInfoWnd / EditWnd / ArrayWnd / EditList OnPaint, HitCell
    ('NCMacroInfoWnd::OnPaint', 'NCMacroInfoWnd', 0x100aae40, 'paint', False),
    ('NCMacroEditWnd::OnPaint', 'NCMacroEditWnd', 0x100aa260, 'paint', False),
    ('NCMacroArrayWnd::OnPaint', 'NCMacroArrayWnd', 0x100a9230, 'paint', False),
    ('NCMacroEditList::OnPaint', 'NCMacroEditList', 0x100ab690, 'paint', False),
    ('NCMacroArrayWnd::HitCell', 'NCMacroArrayWnd', 0x100a8f80, 'int2', False),
    # t_recipe.py: the recipe item window's paint
    ('NCRecipeManufactureItemWnd::OnPaint', 'NCRecipeManufactureItemWnd', 0x100f41b0, 'paint', False),
    # t_shortcut.py: the shortcut bar
    ('NCShortCutWnd::OnPaint', 'NCShortCutWnd', 0x10107b20, 'paint', False),
    ('NCShortCutWnd::SlotAtPoint', 'NCShortCutWnd', 0x101062b0, 'int2', False),
    ('NCShortCutWnd::OnMouseMove', 'NCShortCutWnd', 0x10107c60, 'mouse', False),
    ('NCShortCutWnd::OnLButtonUp', 'NCShortCutWnd', 0x10107df0, 'mouse', False),
    ('NCShortCutWnd::UpdateMacro', 'NCShortCutWnd', 0x10105c60, 'int2', False),
    # t_gm.py: the GM windows' paint
    ('NCGMEquipItemWnd::OnPaint', 'NCGMEquipItemWnd', 0x10089dd0, 'paint', False),
    ('NCGMInventoryWnd::OnPaint', 'NCGMInventoryWnd', 0x1008a5a0, 'paint', False),
    ('NCGMPledgeWnd::OnPaint', 'NCGMPledgeWnd', 0x1008cc50, 'paint', False),
]
# Rows tried and left out of the default run (OLDWND_ALL=1 adds them): the generic world cannot build the state they need, so both sides fault at
# the same instruction of the function (reads / writes through a field the filler leaves null or too small); nothing is compared past that point.
GAPS = [
    ('NCMacroListWnd::OnPaint', 'NCMacroListWnd', 0x100a8c30, 'paint', False),            # ours: /GS cookie trip: the filler's random Macros.Num() prints a longer "(n/24)" than its 14 TCHAR buffer (the game keeps it <= 24)
    ('NCMacroEditList::ValidateLines', 'NCMacroEditList', 0x100aa0c0, 'none', False),      # calls through a null function pointer (the line boxes)
    ('NCRecipeManufactureWnd::OnPaint', 'NCRecipeManufactureWnd', 0x100f4470, 'paint', False),           # reads the inventory pages (t_recipe.py built them by hand)
    ('NCRecipeBuyManufactureWnd::OnPaint', 'NCRecipeBuyManufactureWnd', 0x100f0f50, 'paint', False),     # the same
    ('NCShortCutWnd::PaintBar', 'NCShortCutWnd', 0x10106860, 'paint', True),               # writes through a null slot pointer in 5 cases of 12
    ('NCShortCutWnd::PaintPad', 'NCShortCutWnd', 0x10107400, 'paint', True),               # the same, 4 of 12
    ('NCShortCutWnd::ReloadNames', 'NCShortCutWnd', 0x10106330, 'none', True),             # reads a null slot entry
    ('NCEquipItemWnd::OnPaint', 'NCEquipItemWnd', 0x10094080, 'paint', False),             # reads the equipped items (t_equippaint.py built them with t_fill's item maker)
    ('NCEquipItemWnd::HitTestHenna', 'NCEquipItemWnd', 0x100949b0, 'int2', False),         # reads through the result of the virtual 43
]
for row in OLD + (GAPS if os.environ.get('OLDWND_ALL') else []):
    T(*row, skip=FOLDED)

if __name__ == '__main__':
    t_sys.main()
