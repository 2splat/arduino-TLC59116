env LD_LIBRARY_PATH=`llvm-config-3.3 --libdir` python - "$@" <<EOF
#!/usr/bin/env python
#===- cindex-dump.py - cindex/Python Source Dump -------------*- python -*--===#
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

"""
A simple command line tool for dumping a source file using the Clang Index
Library.
"""
import re
from pprint import pprint

def get_diag_info(diag):
    return [ diag.spelling, diag.location, "severity:%s" % diag.severity ]
    return { 'severity' : diag.severity,
             'location' : diag.location,
             'spelling' : diag.spelling,
             # 'ranges' : diag.ranges,
             # 'fixits' : diag.fixits 
             }

def get_cursor_id(cursor, cursor_list = []):
    if not opts.showIDs:
        return None

    if cursor is None:
        return None

    # FIXME: This is really slow. It would be nice if the index API exposed
    # something that let us hash cursors.
    for i,c in enumerate(cursor_list):
        if cursor == c:
            return i
    cursor_list.append(cursor)
    return len(cursor_list) - 1

        
def print_node(node, depth):
    path = [ (x.spelling if x.spelling else '') for x in depth]
    pprint({ 
             '||' : len(depth),
             '.' : ".".join(path),
             'kind' : node.kind,
             'usr' : node.get_usr(),
             'spelling' : node.spelling,
             'displayname' : node.displayname,
             'raw_comment' : (node.brief_comment if hasattr(node, 'brief_comment') else ''),
             'location' : node.location,
             'extent.start' : node.extent.start,
             'extent.end' : node.extent.end,
             'is_definition' : node.is_definition(),
             'definition id' : get_cursor_id(node.get_definition()),
             })

def get_info(node, depth=0):
    if opts.maxDepth is not None and depth >= opts.maxDepth:
        children = None
    else:
        children = [get_info(c, depth+1)
                    for c in node.get_children()]
    return { 'id' : get_cursor_id(node),
             'kind' : node.kind,
             'usr' : node.get_usr(),
             'spelling' : node.spelling,
             'location' : node.location,
             'extent.start' : node.extent.start,
             'extent.end' : node.extent.end,
             'is_definition' : node.is_definition(),
             'definition id' : get_cursor_id(node.get_definition()),
             'children' : children }

class CppDoc:
    # we know about c++
    
    def __init__(self, filename, include_paths=[], clang_args=[], visitor=None, max_depth=None):
        from clang.cindex import Index, TranslationUnit
        from glob import glob

        for x in include_paths:
            for p in glob(x):
                clang_args.extend( ('-I', p) )
        index = Index.create()
        # print "FNAME %s\n" % filename
        self.tu = index.parse(filename, clang_args) # parse everything so we can find white-space-comments
        if not self.tu:
            parser.error("unable to load input")
        
        self.max_depth = max_depth
        self.filename = filename
        self.extents = []
        self.visitor = visitor(self)

        pprint(('diags', map(get_diag_info, (d for d in self.tu.diagnostics if not re.search('__progmem__',d.spelling)))))
        # pprint(('nodes', get_info(this.tu.cursor)))
        self.visit(self.tu.cursor)

    def visit(self, node, depth=[]):
        if self.max_depth is not None and len(depth) > self.max_depth:
            print "<skip>"
            return
        elif not node.location.file or (self.filename == node.location.file.name):
            if node.location.file:
                self.source_text.parsed(node, node.extent)
                for tok in node.get_tokens():
                    if tok.spelling.startswith('//') or tok.spelling.startswith('/*'):
                        # print "  T %s" % tok.spelling
                        self.append_extent(tok.extent)
                    else:
                        self.source_text.parsed(tok, tok.extent)
                        
                print_node(node, depth) if self.visitor==None else self.visitor.each(node, depth)

            for x in node.get_children():
                path = depth[:]
                path.append(node)
                self.visit(x, path)

    def file_chunk(self, source_range):
        if not hasattr(self, '_file_chunks'):
            with open(self.filename) as f:
                self._file_chunks = f.readlines()
            # print "read %s lines" % len(self._file_chunks)
        if len(self._file_chunks) >= source_range.end.line:
            rez = []
            # print "..hunt %s:%s" % (source_range.start.line , source_range.end.line)
            # for a_line in self._file_chunks[ source_range.start.line-1 : source_range.end.line ]:
            source_first = source_range.start.line-1
            source_last = source_range.end.line-1
            print "source_last %s" % source_last
            for i in range( source_first,source_last+1):
                a_line = self._file_chunks[i]
                line_start = source_range.start.column-1 if i==source_first else 0
                line_end = source_range.end.column if i==source_last else len(a_line)
                print ":: %s:%s<%s> %s" % (line_start, line_end, i, a_line[ line_start:  line_end ])
                rez.append( a_line[ line_start :  line_end])
            return(rez)
        else:
            print "Noooo: have %s lines from %s, but wanted %s" % (len(self._file_chunks),self.filename, source_range)

    def print_extents(self):
        import sys
        print "----"
        for se in self.extents:
            # print "%s:%s - %s:%s" % (se.start.line,se.start.column, se.end.line,se.end.column)
            sys.stdout.write( " ".join(self.file_chunk(se)))

    def append_extent(self, e):
        # print "X+[%s] %-4s %-4s\n" % (len(self.extents),e.start,e.end)
        if len(self.extents) == 0:
            self.extents.append(e)
        else:
            i = -1
            merged = False
            while len(self.extents) >= abs(i) and self.extents[i].start.line >= e.start.line:
                # print "merge? %s\ninto [%s]%s" % (e, i, self.extents[i])
                # print "## %s" % self.extents[i].start.line
                se = self.extents[i]
                if se.start.line == e.start.line:
                    # print "..merge?"
                    se_range = [se.start.column, se.end.column]
                    if e.start.column in se_range or e.end.column in se_range:
                        # print "...merge!"
                        merged = True
                i = i - 1
            if not merged:
                self.extents.append(e)

class CppDoc_Empty:
    def __init__():
        pass

class CppDoc_Visitors:
    class DeclVisitor:
        def __init__(self,translationUnit):
            self.tu = translationUnit

        def each(self,node,depth):
            # print "visited %s" % node
            if node.kind.is_declaration:
                text = " ".join(x.spelling for x in node.get_tokens())
                print "[%d] %s" % (node.extent.start.line,text)
            else:
                print "skip"
            # print_node(node,depth)

    class Raw:
        def __init__(self,translationUnit):
            self.tu = translationUnit

        def standard_methods(self):
            if not hasattr(self,'_standard_methods'):
                self._standard_methods = CppDoc_Empty.__dict__.keys()
            return self._standard_methods

        def describe(self,node,depth):
            import re
            import clang
            out = []
            out.append("%s:%s - %s:%s" % (node.extent.start.line,node.extent.start.column,node.extent.end.line,node.extent.end.column))
            name = []
            if hasattr(node,'displayname') and node.displayname:
                name.append(node.displayname)
            if hasattr(node,'spelling') and node.spelling:
                name.append('spelling:%s' % node.spelling)
            if len(name) > 0:
                out.append(name)

            if hasattr(node,'brief_comment') and node.brief_comment:
                out.append("// " + brief_comment)
            if hasattr(node,'raw_comment') and node.raw_comment:
                out.append(["raw_comment",raw_comment])

            # get_arguments is an iter of cursor

            type = []
            if node.result_type and node.result_type.kind != clang.cindex.TypeKind.INVALID:
                # gives a Class Type
                out.append(['result_type',self.describe_type(node.result_type)])
            if hasattr(node,'access_specifier') and node.access_specifier:
                type.append("access_specifier:%s" % node.access_specifier)
            if node.is_static_method():
                type.append('static')
            if hasattr(node,'storage_class') and node.storage_class:
                type.append("storage_class:%s" % storage_class)
            if node.is_definition():
                type.append('defn')
            if node.kind == clang.cindex.CursorKind.ENUM_DECL:
                # a Class Type...
                type.append("enum_type=%s",node.enum_type)
            if node.kind == clang.cindex.CursorKind.ENUM_CONSTANT_DECL:
                type.append("enum_value=%s",node.enum_value)
            if node.is_bitfield():
                type.append("bitfield[%s]" % node.get_bitfield_width())
            if len(type)>0:
                out.append(" ".join(type))
            if depth != -1 and node.type and node.type.kind != clang.cindex.TypeKind.INVALID :
                out.append( self.describe_type(node.type))

            # get_definition gives a cursor if is_definition?
            general_kind=[]
            if node.kind.is_reference():
                general_kind.append('ref')
            if node.kind.is_expression():
                general_kind.append('expr')
            if node.kind.is_declaration():
                general_kind.append('decl')
            if node.kind.is_statement():
                general_kind.append('stmt')
            # more general_kind
            out.append([node.kind, " ".join(general_kind)])
            # out.append( ['methods' ," ".join(sorted(list(x for x in node.__class__.__dict__.keys() if not (x in self.standard_methods()))))])
            return out

        def describe_type(self,node):
            import clang
            out = []
            out.append(node.kind)
            if node.get_declaration() and node.get_declaration().displayname:
                out.append(node.get_declaration().displayname)
            if hasattr(node,'spelling') and node.spelling:
                out.append("SP:"+node.spelling)
            # out.append(node.element_type)
            if node.is_const_qualified():
                out.append('const')
            if node.is_volatile_qualified():
                out.append('volatile')
            if node.is_restrict_qualified():
                out.append('restrict')
            if hasattr(node,'get_ref_qualifier') and node.get_ref_qualifier():
                out.append(['ref_qualifier', node.get_ref_qualifier()])
            if node.get_pointee() and node.get_pointee().kind != clang.cindex.TypeKind.INVALID:
                out.append(["*", self.describe_type(node.get_pointee())])
            if node.get_array_element_type() and node.get_array_element_type().kind != clang.cindex.TypeKind.INVALID:
                out.append(['[%s]' % node.get_array_size(), self.describe_type(node.get_array_element_type())])
            if hasattr(node,'get_class_type') and node.get_class_type():
                out.append(['class_type', node.get_class_type()])

            return out

        def each(self,node,depth):
            pprint(self.describe(node,depth))




def main():
    import sys

    from optparse import OptionParser, OptionGroup

    global opts

    parser = OptionParser("usage: %prog [options] {filename} [clang-args*]")
    parser.add_option("", "--show-ids", dest="showIDs",
                      help="Don't compute cursor IDs (very slow)",
                      default=False)
    parser.add_option("", "--include-paths", dest="includePaths",
                      help="Where to look for #includes (comma list w/globs)",
                      metavar="dirs", type=str, default=None)
    parser.add_option("", "--max-depth", dest="maxDepth",
                      help="Limit cursor expansion to depth N",
                      metavar="N", type=int, default=None)
    parser.add_option("", "--visitor", dest="visitorClass",
                      help="File/class to process each node",
                      metavar="class/file", type=str, default=CppDoc_Visitors.DeclVisitor)
    parser.disable_interspersed_args()
    (opts, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('Expected input-file-name')

    vc = opts.visitorClass
    if isinstance(vc,basestring):
        pieces = vc.split('.')
        print "pieces %s" % pieces
        first_piece = pieces.pop(0)
        vc = None
        if first_piece in globals():
            vc = globals()[first_piece] # getattr(__main__, pieces.pop(0))
        elif hasattr(CppDoc_Visitors, first_piece):
            vc = getattr(CppDoc_Visitors, first_piece)
        if vc==None:
            raise Exception("No such class %s (or CppDoc_Visitors.%s)" % (first_piece,first_piece))
        while len(pieces) > 0:
            print "..next %s.%s" % (vc,pieces)
            vc = getattr(vc, pieces.pop(0))
    print "Visitor %s" % vc
    parsed = CppDoc(args[0], opts.includePaths.split(','), visitor=vc)
    parsed.print_extents()

if __name__ == '__main__':
    main()
EOF
