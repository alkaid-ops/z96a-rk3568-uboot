// @category Z96A
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;
import java.io.*;

public class TraceUboot extends GhidraScript {
    public void run() throws Exception {
        try (PrintWriter out = new PrintWriter(new FileWriter(getScriptArgs()[0]))) {
            for (String text : new String[]{"00a03e14", "00a03064", "00a02d10", "00a68844", "00a68944", "00afc84d", "00aeedcc"}) {
                Address a = toAddr(text);
                out.println("Target " + a);
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(a);
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Function f = getFunctionContaining(ref.getFromAddress());
                    out.println("  from " + ref.getFromAddress() + " type " + ref.getReferenceType() +
                        " function " + (f == null ? "none" : f.getName() + " @ " + f.getEntryPoint()));
                }
            }
        }
    }
}
