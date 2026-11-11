// Export generated pseudocode and references for offline investigation.
// @category Z96A
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class ExportUboot extends GhidraScript {
    public void run() throws Exception {
        String dir = getScriptArgs()[0];
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter c = new PrintWriter(new FileWriter(dir + "/uboot-decompiled.c"));
             PrintWriter refs = new PrintWriter(new FileWriter(dir + "/wifi-string-references.txt"))) {
            FunctionIterator funcs = currentProgram.getFunctionManager().getFunctions(true);
            while (funcs.hasNext() && !monitor.isCancelled()) {
                Function f = funcs.next();
                DecompileResults r = decompiler.decompileFunction(f, 30, monitor);
                c.println("\n/* " + f.getEntryPoint() + " " + f.getName() + " */");
                if (r.decompileCompleted()) c.println(r.getDecompiledFunction().getC());
                else c.println("/* decompilation failed: " + r.getErrorMessage() + " */");
            }
            DataIterator data = currentProgram.getListing().getDefinedData(true);
            while (data.hasNext() && !monitor.isCancelled()) {
                Data d = data.next();
                Object v = d.getValue();
                if (!(v instanceof String)) continue;
                String s = ((String)v).toLowerCase();
                if (!(s.contains("wifi") || s.contains("wlan") || s.contains("wifibt") || s.contains("vcc9-supply"))) continue;
                refs.println(d.getAddress() + " " + v);
                ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(d.getAddress());
                while (ri.hasNext()) {
                    Reference ref = ri.next();
                    Function f = getFunctionContaining(ref.getFromAddress());
                    refs.println("  " + ref.getFromAddress() + " " + (f == null ? "no function" : f.getName() + " @ " + f.getEntryPoint()));
                }
            }
        } finally { decompiler.dispose(); }
    }
}
