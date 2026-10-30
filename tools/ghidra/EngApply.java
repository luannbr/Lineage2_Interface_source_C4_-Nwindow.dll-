// Annotate the Engine.dll analysis (tools/engine/annotate.sh):
//  1. The Engine is an /INCREMENTAL build: every export is a 5-byte `jmp` stub (the ILT) and Ghidra
//     names the stub, not the function behind it. For each stub: the function it jumps to takes the
//     stub's name, class (namespace), calling convention and signature, and the stub becomes a thunk
//     of it. 6,802 named functions with their C++ signatures instead of FUN_xxxxxxxx.
//  2. The class layouts of tools/engine/layouts.py (analysis/engine/layouts.tsv): one structure per
//     class in /Engine, every field of the class and its ancestors at its offset. The structure the
//     demangler made for the same class (empty, /Demangler) takes its content, so `this` in every
//     method of the class shows `this->CollisionRadius` in the decompiler.
// args: <layouts.tsv>
//@category NWindowDecomp
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class EngApply extends GhidraScript {

    private DataType fieldType(String type, int size) {
        if (size == 4) {
            if (type.equals("float")) return FloatDataType.dataType;
            if (type.startsWith("object") || type.startsWith("class")) return new PointerDataType(VoidDataType.dataType);
            if (type.equals("bool") || type.equals("bits")) return UnsignedIntegerDataType.dataType;
            return IntegerDataType.dataType;
        }
        if (size == 1) return ByteDataType.dataType;
        if (size == 2) return ShortDataType.dataType;
        return new ArrayDataType(ByteDataType.dataType, size, 1);
    }

    @Override
    public void run() throws Exception {
        // ---- 1. names behind the ILT stubs ----
        int renamed = 0, signed = 0, skipped = 0, failed = 0;
        List<Function> stubs = new ArrayList<>();
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            if (f.getBody().getNumAddresses() == 5 && !f.getName().startsWith("FUN_") && !f.getName().startsWith("thunk_"))
                stubs.add(f);
        }
        for (Function stub : stubs) {
            Instruction ins = getInstructionAt(stub.getEntryPoint());
            if (ins == null || !ins.getMnemonicString().equalsIgnoreCase("JMP") || ins.getFlows().length != 1) {
                skipped++;
                continue;
            }
            Address tgt = ins.getFlows()[0];
            Function t = getFunctionAt(tgt);
            if (t == null || t.equals(stub) || !t.getName().startsWith("FUN_")) {
                skipped++;
                continue;
            }
            try {
                Namespace ns = stub.getParentNamespace();
                String name = stub.getName();
                String cc = stub.getCallingConventionName();
                t.setParentNamespace(ns);
                t.setName(name, SourceType.IMPORTED);
                if (cc != null && !cc.equals("unknown")) t.setCallingConvention(cc);
                renamed++;
                try {
                    new ApplyFunctionSignatureCmd(t.getEntryPoint(), stub.getSignature(), SourceType.IMPORTED, false, true)
                        .applyTo(currentProgram);
                    signed++;
                } catch (Exception e) { /* the name stays, the signature does not */ }
                try { stub.setThunkedFunction(t); } catch (Exception e) { }
            } catch (Exception e) {
                failed++;
            }
        }
        println("ILT stubs: " + stubs.size() + ", targets named " + renamed + ", signatures " + signed
            + ", skipped " + skipped + ", failed " + failed);

        // ---- 2. class structures ----
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        Map<String, StructureDataType> structs = new HashMap<>();
        try (BufferedReader r = new BufferedReader(new FileReader(getScriptArgs()[0]))) {
            String line;
            StructureDataType cur = null;
            while ((line = r.readLine()) != null) {
                String[] p = line.split("\t");
                if (p[0].equals("class")) {
                    cur = new StructureDataType(new CategoryPath("/Engine"), p[1], 0);
                    cur.setLength(Integer.parseInt(p[3]));
                    structs.put(p[1], cur);
                } else if (p[0].equals("field") && cur != null) {
                    int off = Integer.parseInt(p[2]), size = Integer.parseInt(p[3]);
                    if (off + size > cur.getLength()) continue;
                    // `bits`: the 32-bit word the consecutive bools of a class share; the comment says
                    // which bit is which bool ("0:bNetInitial 1:bNetOwner ..."), so a decompiled
                    // `this->bits_bNetInitial & 2` reads as bNetOwner
                    String comment = p.length > 6 && !p[6].isEmpty() ? p[6] : p[4];
                    try {
                        cur.replaceAtOffset(off, fieldType(p[4], size), size, p[5], comment);
                    } catch (Exception e) { /* an overlap with a field of a base class at the same place */ }
                }
            }
        }
        int made = 0, wired = 0;
        for (Map.Entry<String, StructureDataType> e : structs.entrySet()) {
            DataType placed = dtm.addDataType(e.getValue(), DataTypeConflictHandler.REPLACE_HANDLER);
            made++;
            List<DataType> found = new ArrayList<>();
            dtm.findDataTypes(e.getKey(), found);
            for (DataType d : found) {
                if (d != placed && d instanceof Structure && d.getCategoryPath().toString().startsWith("/Demangler")) {
                    try {
                        ((Structure) d).replaceWith(e.getValue());
                        wired++;
                    } catch (Exception ex) { }
                }
            }
        }
        println("class structures: " + made + " in /Engine, " + wired + " demangler types filled");
    }
}
