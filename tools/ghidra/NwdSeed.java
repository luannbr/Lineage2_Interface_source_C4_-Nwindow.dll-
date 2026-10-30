// Create functions at the addresses listed in analysis/seeds.txt (tools/gen_seeds.py) that the
// auto-analysis did not reach (methods only referenced from vtables / message maps / tables).
// args: <seeds.txt>
//@category NWindowDecomp
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.io.BufferedReader;
import java.io.FileReader;

public class NwdSeed extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        int made = 0, skipped = 0, failed = 0;
        try (BufferedReader r = new BufferedReader(new FileReader(args[0]))) {
            String line;
            while ((line = r.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty())
                    continue;
                Address a = toAddr(Long.parseLong(line, 16));
                if (getFunctionAt(a) != null) {
                    skipped++;
                    continue;
                }
                Function inside = getFunctionContaining(a);
                if (inside != null) {
                    skipped++;
                    continue;
                }
                if (getInstructionAt(a) == null) {
                    DisassembleCommand cmd = new DisassembleCommand(a, null, true);
                    cmd.applyTo(currentProgram, monitor);
                }
                Function f = createFunction(a, null);
                if (f != null)
                    made++;
                else
                    failed++;
            }
        }
        println("seeded functions: made=" + made + " skipped=" + skipped + " failed=" + failed);
    }
}
