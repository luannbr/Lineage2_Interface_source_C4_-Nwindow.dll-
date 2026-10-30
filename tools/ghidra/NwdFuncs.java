// Light export: one row per function (va, size, ranges, name, symbol source, thunk) + program stats.
//@category NWindowDecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.charset.StandardCharsets;

public class NwdFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args[0]);
        out.mkdirs();
        PrintWriter w = new PrintWriter(new OutputStreamWriter(new FileOutputStream(new File(out, "funcs.tsv")), StandardCharsets.UTF_8));
        w.println("va\tsize\tranges\tname\tsrc\tthunk\tnparams\tcallconv");
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        int n = 0;
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.isExternal()) continue;
            n++;
            AddressSetView body = f.getBody();
            Symbol s = f.getSymbol();
            StringBuilder rg = new StringBuilder();
            for (AddressRange r : body.getAddressRanges()) {
                if (rg.length() > 0) rg.append(',');
                rg.append(String.format("%08x-%08x", r.getMinAddress().getOffset(), r.getMaxAddress().getOffset()));
            }
            w.println(String.format("%08x\t%d\t%s\t%s\t%s\t%d\t%d\t%s", f.getEntryPoint().getOffset(),
                body.getNumAddresses(), rg.toString(), f.getName(true), s.getSource().toString(),
                f.isThunk() ? 1 : 0, f.getParameterCount(), f.getCallingConventionName()));
        }
        w.close();
        println("functions: " + n);
    }
}
