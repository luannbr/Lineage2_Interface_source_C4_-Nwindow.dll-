// Headless export of the whole NWindow.dll analysis: one TSV row per function plus
// the decompiled C and the disassembly of each function (analysis/ghidra/...).
// Run by tools/ghidra/run_export.sh; args: <outDir>
//@category NWindowDecomp
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.Set;

public class NwdExport extends GhidraScript {

    private static PrintWriter open(File f) throws Exception {
        return new PrintWriter(new OutputStreamWriter(new FileOutputStream(f), StandardCharsets.UTF_8));
    }

    private static String hex(Address a) {
        return String.format("%08x", a.getOffset());
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : "analysis/ghidra");
        File decDir = new File(out, "decomp");
        File asmDir = new File(out, "asm");
        decDir.mkdirs();
        asmDir.mkdirs();

        DecompInterface di = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        di.setOptions(opts);
        di.toggleCCode(true);
        di.toggleSyntaxTree(false);
        di.setSimplificationStyle("decompile");
        di.openProgram(currentProgram);

        PrintWriter tsv = open(new File(out, "functions.tsv"));
        tsv.println("va\tsize\tranges\tname\tthunk\tcallconv\tnparams\tcallers\tcallees\tdecomp_ok\tsignature");

        int total = currentProgram.getFunctionManager().getFunctionCount();
        int n = 0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.isExternal())
                continue;
            n++;
            String va = hex(f.getEntryPoint());
            AddressSetView body = f.getBody();
            long size = body.getNumAddresses();
            StringBuilder ranges = new StringBuilder();
            for (AddressRange r : body.getAddressRanges()) {
                if (ranges.length() > 0)
                    ranges.append(',');
                ranges.append(hex(r.getMinAddress())).append('-').append(hex(r.getMaxAddress()));
            }
            Set<Function> callers = f.getCallingFunctions(monitor);
            Set<Function> callees = f.getCalledFunctions(monitor);
            StringBuilder cs = new StringBuilder();
            for (Function c : callees) {
                if (cs.length() > 0)
                    cs.append(',');
                cs.append(c.isExternal() ? c.getName() : hex(c.getEntryPoint()));
            }

            // disassembly
            try (PrintWriter a = open(new File(asmDir, va + ".asm"))) {
                a.println("; " + f.getName() + "  " + ranges);
                InstructionIterator ii = currentProgram.getListing().getInstructions(body, true);
                while (ii.hasNext()) {
                    Instruction ins = ii.next();
                    StringBuilder bytes = new StringBuilder();
                    for (byte b : ins.getBytes())
                        bytes.append(String.format("%02x", b & 0xff));
                    StringBuilder refs = new StringBuilder();
                    for (Reference ref : ins.getReferencesFrom()) {
                        if (ref.getToAddress().isMemoryAddress()) {
                            Function tf = getFunctionAt(ref.getToAddress());
                            String lab = tf != null ? tf.getName() : null;
                            if (lab == null && getSymbolAt(ref.getToAddress()) != null)
                                lab = getSymbolAt(ref.getToAddress()).getName();
                            if (lab != null)
                                refs.append(" ; ").append(lab);
                        }
                    }
                    a.println(String.format("%s  %-24s %s%s", hex(ins.getAddress()), bytes, ins.toString(), refs));
                }
            }

            // decompile
            boolean ok = false;
            try {
                DecompileResults res = di.decompileFunction(f, 90, monitor);
                if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                    try (PrintWriter d = open(new File(decDir, va + ".c"))) {
                        d.print(res.getDecompiledFunction().getC());
                    }
                    ok = true;
                }
            } catch (Exception e) {
                println("decompile failed " + va + ": " + e);
            }

            tsv.println(String.join("\t", va, Long.toString(size), ranges.toString(), f.getName(true),
                    f.isThunk() ? "1" : "0", String.valueOf(f.getCallingConventionName()),
                    Integer.toString(f.getParameterCount()), Integer.toString(callers.size()), cs.toString(),
                    ok ? "1" : "0", f.getSignature().getPrototypeString().replace('\t', ' ')));
            if (n % 250 == 0)
                println("exported " + n + " / " + total);
        }
        tsv.close();
        di.dispose();
        println("done: " + n + " functions");
    }
}
