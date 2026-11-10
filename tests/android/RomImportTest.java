package io.github.mamesj.pokemonds;
import java.io.*;
import java.nio.file.Files;
import java.util.Arrays;
public class RomImportTest {
    static void check(boolean value) { if(!value)throw new AssertionError(); }
    static void fails(IORun call) throws Exception { try { call.run();throw new AssertionError("expected failure"); }catch(IOException expected){} }
    interface IORun { void run() throws Exception; }
    public static void main(String[] args) throws Exception {
        File root=new File(args[0]);root.mkdirs();
        byte[] first=new byte[4096], second=new byte[4096];Arrays.fill(first,(byte)17);Arrays.fill(second,(byte)33);
        File a=RomImport.copy(new ByteArrayInputStream(first),"content://provider/a","My Game.NDS",root);
        File b=RomImport.copy(new ByteArrayInputStream(second),"content://provider/b","My Game.NDS",root);
        check(!a.equals(b));check(Arrays.equals(Files.readAllBytes(a.toPath()),first));
        File again=RomImport.copy(new ByteArrayInputStream(second),"content://provider/a","My Game.NDS",root);
        check(again.equals(a));check(Arrays.equals(Files.readAllBytes(a.toPath()),second));
        fails(()->RomImport.copy(new ByteArrayInputStream(new byte[40]),"content://provider/a","My Game.NDS",root));
        check(Arrays.equals(Files.readAllBytes(a.toPath()),second));
        fails(()->RomImport.copy(new InputStream(){int count;public int read()throws IOException {if(count++>800)throw new IOException("provider disconnected");return 1;}},"content://provider/a","My Game.NDS",root));
        check(Arrays.equals(Files.readAllBytes(a.toPath()),second));
        for(String name:new String[]{"../a.nds","a/b.nds","a"+(char)92+"b.nds","a\n.nds","game.sav","",null})fails(()->RomImport.name(name));
        for(File folder:root.listFiles())for(File file:folder.listFiles())check(!file.getName().endsWith(".part"));
        check(RomImport.key("content://provider/a").equals(RomImport.key("content://provider/a")));
        System.out.println("PASS: provider imports, duplicate filenames, atomic replacement and interrupted/invalid streams preserve prior data");
    }
}
