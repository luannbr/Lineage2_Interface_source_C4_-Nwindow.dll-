// Decompile the functions whose full name (Class::Method) matches a pattern and print the C.
// args: <outFile> <regex> [<regex> ...]      e.g. out.c "AActor::GetCylinderExtent" "AActor::SetCollisionSize"
//@category NWindowDecomp
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.PrintWriter;
import java.util.regex.Pattern;

public class NwdDecompNamed extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(a[0], "UTF-8")) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                String n = f.getName(true);
                for (int i = 1; i < a.length; i++) {
                    if (Pattern.compile(a[i]).matcher(n).find() && f.getBody().getNumAddresses() > 5) {
                        DecompileResults r = di.decompileFunction(f, 60, monitor);
                        w.println("// " + n + " @ " + f.getEntryPoint());
                        w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed");
                        break;
                    }
                }
            }
        }
    }
}
