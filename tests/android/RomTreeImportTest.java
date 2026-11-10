package io.github.mamesj.pokemonds;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
public class RomTreeImportTest {
    static void check(boolean value){if(!value)throw new AssertionError();}
    public static void main(String[] args)throws Exception {
        File root=new File(args[0]);root.mkdirs();
        byte[] bytes=new byte[1024];Arrays.fill(bytes,(byte)9);
        Map<String,List<RomTreeImport.Entry>> nodes=new HashMap<>();
        nodes.put("root",Arrays.asList(new RomTreeImport.Entry("nested","folder",true),new RomTreeImport.Entry("save","Game.sav",false),new RomTreeImport.Entry("one","Game.nds",false)));
        nodes.put("nested",Arrays.asList(new RomTreeImport.Entry("root","cycle",true),new RomTreeImport.Entry("two","Game.nds",false),new RomTreeImport.Entry("gba","Emerald.gba",false)));
        RomTreeImport.Provider provider=new RomTreeImport.Provider(){
            public List<RomTreeImport.Entry> children(String id){return nodes.get(id);}
            public InputStream open(String id){check(!id.equals("save"));return new ByteArrayInputStream(bytes);}
        };
        RomTreeImport.Result result=RomTreeImport.copy(provider,"content://tree","root",root);
        check(result.count==3);Set<String> paths=new HashSet<>();
        for(File bucket:result.folder.listFiles())for(File file:bucket.listFiles()) {paths.add(file.getPath());check(Arrays.equals(Files.readAllBytes(file.toPath()),bytes));}
        check(paths.size()==3);
        check(RomTreeImport.copy(provider,"content://tree","root",root).count==3);
        nodes.put("root",Collections.emptyList());check(RomTreeImport.copy(provider,"empty","root",root).count==0);
        List<RomTreeImport.Entry> big=new ArrayList<>();for(int i=0;i<4097;i++)big.add(new RomTreeImport.Entry("x"+i,"note.txt",false));nodes.put("root",big);
        try{RomTreeImport.copy(provider,"huge","root",root);throw new AssertionError();}catch(IOException expected){}
        nodes.clear();for(int i=0;i<18;i++)nodes.put("d"+i,Arrays.asList(new RomTreeImport.Entry("d"+(i+1),"deep",true)));
        try{RomTreeImport.copy(provider,"deep","d0",root);throw new AssertionError();}catch(IOException expected){}
        System.out.println("PASS: nested DS/GBA tree import, cycles, duplicate names, empty trees and depth/listing bounds; saves never opened");
    }
}
