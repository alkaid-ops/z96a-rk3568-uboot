// Export vendor U-Boot charger-related strings, xrefs, and pseudocode.
// @category Z96A
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class ExportCharger extends GhidraScript {
    private boolean interesting(String s) {
        String l = s.toLowerCase();
        return l.contains("z96a-pwr") || l.contains("sc8886") ||
               l.contains("bat_remov") || l.contains("charge current") ||
               l.contains("vindpm") || l.contains("bq257");
    }

    public void run() throws Exception {
        String dir = getScriptArgs()[0];
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Long> seen = new HashSet<>();
        try (PrintWriter refs = new PrintWriter(new FileWriter(dir + "/charger-string-references.txt"));
             PrintWriter code = new PrintWriter(new FileWriter(dir + "/charger-functions.c"))) {
            DataIterator data = currentProgram.getListing().getDefinedData(true);
            while (data.hasNext() && !monitor.isCancelled()) {
                Data d = data.next();
                Object v = d.getValue();
                if (!(v instanceof String)) continue;
                String s = (String)v;
                if (!interesting(s)) continue;
                refs.println(d.getAddress() + " " + s);
                ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(d.getAddress());
                while (ri.hasNext()) {
                    Reference ref = ri.next();
                    Function f = getFunctionContaining(ref.getFromAddress());
                    refs.println("  xref " + ref.getFromAddress() + " " +
                        (f == null ? "no function" : f.getName() + " @ " + f.getEntryPoint()));
                    if (f != null && seen.add(f.getEntryPoint().getOffset())) {
                        DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
                        code.println("\n/* " + f.getEntryPoint() + " " + f.getName() + " */");
                        if (r.decompileCompleted()) {
                            code.println(r.getDecompiledFunction().getC());
                        } else {
                            code.println("/* decompilation failed: " + r.getErrorMessage() + " */");
                        }
                        FunctionIterator callers = currentProgram.getFunctionManager().getFunctions(true);
                        // Direct caller discovery is handled below for each ref function.
                    }
                }
            }
            // Also emit direct callers and callees for every referring function.
            FunctionIterator funcs = currentProgram.getFunctionManager().getFunctions(true);
            while (funcs.hasNext() && !monitor.isCancelled()) {
                Function f = funcs.next();
                if (seen.contains(f.getEntryPoint().getOffset())) {
                    refs.println("function " + f.getEntryPoint() + " " + f.getName());
                    for (Function caller : f.getCallingFunctions(monitor)) {
                        refs.println("  caller " + caller.getEntryPoint() + " " + caller.getName());
                    }
                    for (Function callee : f.getCalledFunctions(monitor)) {
                        refs.println("  callee " + callee.getEntryPoint() + " " + callee.getName());
                    }
                }
            }
        } finally {
            decompiler.dispose();
        }
    }
}
