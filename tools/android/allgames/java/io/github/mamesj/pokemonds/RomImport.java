package io.github.mamesj.pokemonds;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Locale;

// Content providers need not expose a filesystem path. Keep their ROM copies in an app-owned
// directory; stage the complete stream before replacing an older copy of the same document.
final class RomImport {
    static boolean isDs(String name) { return name != null && name.toLowerCase(Locale.ROOT).endsWith(".nds"); }

    static String key(String source) {
        try {
            byte[] hash = MessageDigest.getInstance("SHA-256").digest(source.getBytes(StandardCharsets.UTF_8));
            StringBuilder text = new StringBuilder();
            for (byte b : hash) text.append(String.format(Locale.ROOT, "%02x", b & 255));
            return text.toString();
        } catch (Exception e) { throw new IllegalStateException(e); }
    }

    static String name(String name) throws IOException {
        if (!isDs(name) || name.length() > 200 || name.indexOf('/') >= 0 || name.indexOf((char)92) >= 0)
            throw new IOException("Choose a Nintendo DS ROM (.nds)");
        for (int i=0;i<name.length();++i) if (name.charAt(i)<32) throw new IOException("Invalid ROM filename");
        return name;
    }

    static File copy(InputStream input, String source, String displayName, File root) throws IOException {
        String leaf = name(displayName);
        File folder = new File(root, key(source));
        if (!folder.isDirectory() && !folder.mkdirs()) throw new IOException("Cannot create ROM import folder");
        File destination = new File(folder, leaf);
        File staged = File.createTempFile("rom-", ".part", folder);
        boolean finished = false;
        try {
            long count=0;
            byte[] buffer=new byte[1024*1024];
            try (FileOutputStream output=new FileOutputStream(staged)) {
                int n;
                while ((n=input.read(buffer))!=-1) {
                    if (n==0) throw new IOException("ROM provider returned no data");
                    count+=n;
                    if (count>1024L*1024*1024) throw new IOException("ROM exceeds the 1 GB import limit");
                    output.write(buffer,0,n);
                }
                if (count<512) throw new IOException("ROM is too small or incomplete");
                output.getFD().sync();
            }
            // Same filesystem: a failed rename leaves the previous ROM in place.
            if (!staged.renameTo(destination)) throw new IOException("Cannot finish ROM import");
            finished=true;
            return destination;
        } finally { if (!finished) staged.delete(); }
    }
}
