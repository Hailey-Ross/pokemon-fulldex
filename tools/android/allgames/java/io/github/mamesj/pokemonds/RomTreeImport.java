package io.github.mamesj.pokemonds;
import java.io.*;
import java.util.*;

// Provider-independent traversal keeps cycles, deep trees and huge listings bounded. A folder
// is selected only after the complete import succeeds; no source file or save is written.
final class RomTreeImport {
    static final class Entry {
        final String id, name;
        final boolean directory;
        Entry(String id,String name,boolean directory) { this.id=id;this.name=name;this.directory=directory; }
    }
    interface Provider {
        List<Entry> children(String document) throws IOException;
        InputStream open(String document) throws IOException;
    }
    static final class Result {
        final File folder;
        final int count;
        Result(File folder,int count) {this.folder=folder;this.count=count;}
    }
    static final class Node {
        final String id;final int depth;
        Node(String id,int depth){this.id=id;this.depth=depth;}
    }
    static Result copy(Provider provider,String source,String rootId,File imports) throws IOException {
        File folder=new File(imports,RomImport.key(source));
        Set<String> visited=new HashSet<>();ArrayDeque<Node> pending=new ArrayDeque<>();
        pending.add(new Node(rootId,0));int seen=0,count=0;long bytes=0;
        while(!pending.isEmpty()) {
            Node node=pending.removeFirst();
            if(!visited.add(node.id))continue;
            if(node.depth>16)throw new IOException("ROM folder nesting is too deep; choose a smaller folder");
            List<Entry> entries=provider.children(node.id);
            for(Entry entry:entries) {
                if(++seen>4096)throw new IOException("Too many files; choose a smaller ROM folder");
                if(entry.directory) {pending.add(new Node(entry.id,node.depth+1));continue;}
                if(!RomImport.isRom(entry.name))continue;
                if(count>=256)throw new IOException("Too many ROMs; choose a smaller folder");
                try(InputStream stream=provider.open(entry.id)) {
                    if(stream==null)throw new IOException("A ROM could not be opened by its provider");
                    File rom=RomImport.copy(stream,source+"\n"+entry.id,entry.name,folder);
                    bytes+=rom.length();
                    if(bytes>8L*1024*1024*1024)throw new IOException("ROM folder exceeds 8 GB; choose a smaller folder");
                    count++;
                }
            }
        }
        return new Result(folder,count);
    }
}
