import java.util.HashSet;
import java.util.Set;

public class palabras {
    Set<String> palabraDiferentes(String[] entrada) {
        Set<String> salida = new HashSet<>();
        for (String s : entrada) {
            salida.add(s);
        }
        return salida;
    }
}


