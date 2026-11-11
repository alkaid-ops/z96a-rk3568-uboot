// Decompile selected vendor U-Boot functions.
// @category Z96A
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportSelected extends GhidraScript {
    public void run() throws Exception {
        String dir = getScriptArgs()[0];
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(new FileWriter(dir + "/selected-functions.c"))) {
            for (int i = 1; i < getScriptArgs().length; i++) {
                Address a = toAddr(getScriptArgs()[i]);
                Function f = getFunctionAt(a);
                if (f == null) f = getFunctionContaining(a);
                out.println("\n/* selected " + a + " */");
                if (f == null) {
                    out.println("/* no function */");
                    continue;
                }
                out.println("/* function " + f.getEntryPoint() + " " + f.getName() + " */");
                for (Function caller : f.getCallingFunctions(monitor))
                    out.println("/* caller " + caller.getEntryPoint() + " " + caller.getName() + " */");
                for (Function callee : f.getCalledFunctions(monitor))
                    out.println("/* callee " + callee.getEntryPoint() + " " + callee.getName() + " */");
                DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
                if (r.decompileCompleted()) out.println(r.getDecompiledFunction().getC());
                else out.println("/* decompilation failed: " + r.getErrorMessage() + " */");
            }
        } finally {
            decompiler.dispose();
        }
    }
}
