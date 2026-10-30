// Apply db/functions.csv names to the Ghidra project: "Class::Method" becomes function Method in
// class namespace Class (so the decompiler types `this` as Class*), plain names stay global.
// Only functions still carrying a default name (FUN_/thunk) or an older name of ours are renamed.
// args: <functions.csv>
//@category NWindowDecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.SymbolTable;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public class NwdApplyNames extends GhidraScript {

    private static List<String> splitCsv(String line) {
        List<String> out = new ArrayList<>();
        StringBuilder cur = new StringBuilder();
        boolean q = false;
        for (int i = 0; i < line.length(); i++) {
            char c = line.charAt(i);
            if (q) {
                if (c == '"' && i + 1 < line.length() && line.charAt(i + 1) == '"') {
                    cur.append('"');
                    i++;
                } else if (c == '"') {
                    q = false;
                } else {
                    cur.append(c);
                }
            } else if (c == '"') {
                q = true;
            } else if (c == ',') {
                out.add(cur.toString());
                cur.setLength(0);
            } else {
                cur.append(c);
            }
        }
        out.add(cur.toString());
        return out;
    }

    private static String clean(String s) {
        return s.replace("`", "").replace("'", "").replace(' ', '_');
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        SymbolTable st = currentProgram.getSymbolTable();
        int renamed = 0, skipped = 0, thiscalls = 0;
        try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(args[0]), StandardCharsets.UTF_8))) {
            String header = r.readLine();
            String line;
            while ((line = r.readLine()) != null) {
                List<String> f = splitCsv(line);
                if (f.size() < 5)
                    continue;
                String kind = f.get(2), name = f.get(4);
                if (name.startsWith("sub_") || !(kind.equals("game") || kind.equals("init")))
                    continue;
                Address a = toAddr(Long.parseLong(f.get(0), 16));
                Function fn = getFunctionAt(a);
                if (fn == null) {
                    skipped++;
                    continue;
                }
                String cls = null, meth = name;
                int sep = name.indexOf("::");
                if (sep > 0) {
                    cls = name.substring(0, sep);
                    meth = name.substring(sep + 2);
                }
                meth = clean(meth);
                if (meth.isEmpty())
                    continue;
                Namespace ns = currentProgram.getGlobalNamespace();
                if (cls != null && cls.indexOf('.') < 0) {
                    try {
                        Namespace existing = st.getNamespace(cls, ns);
                        if (existing == null)
                            ns = st.createClass(ns, cls, SourceType.USER_DEFINED);
                        else if (existing.getSymbol().getSymbolType() != ghidra.program.model.symbol.SymbolType.CLASS)
                            ns = st.convertNamespaceToClass(existing);
                        else
                            ns = existing;
                    } catch (Exception e) {
                        println("namespace " + cls + ": " + e.getMessage());
                        continue;
                    }
                } else if (cls != null) {
                    continue;
                }
                try {
                    fn.setParentNamespace(ns);
                    fn.setName(meth, SourceType.USER_DEFINED);
                    renamed++;
                    // a method Ghidra guessed as __fastcall (ecx used, edx not a parameter) is a
                    // __thiscall: switching makes the decompiler type `this` as Class*
                    if (cls != null && "__fastcall".equals(fn.getCallingConventionName())) {
                        ghidra.program.model.listing.Parameter[] ps = fn.getParameters();
                        boolean edx = ps.length > 1 && ps[1].isRegisterVariable()
                                && ps[1].getRegister() != null && ps[1].getRegister().getName().equals("EDX");
                        if (!edx) {
                            fn.setCallingConvention("__thiscall");
                            thiscalls++;
                        }
                    }
                } catch (Exception e) {
                    println("rename failed " + f.get(0) + " " + name + ": " + e.getMessage());
                }
            }
        }
        println("applied names: " + renamed + " (skipped " + skipped + "), __thiscall set on " + thiscalls);
    }
}
