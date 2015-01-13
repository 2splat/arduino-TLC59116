env LD_LIBRARY_PATH=`llvm-config-3.4 --libdir` python - "$@" <<EOF
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
import sys
import clang
    # https://github.com/llvm-mirror/clang/blob/release_34/bindings/python/clang/cindex.py
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

# not a class/nor instance method
def inside_range(a,b):
    # a in [x,y]
    # a can also be [j,k]
    # print "test %s in %s" % (a,b)
    if isinstance(a,list):
        return (b[0] <= a[0] <= b[1]) and (b[0] <= a[1] <= b[1])
    else:
        return b[0] >= a >= b[1]

class CppDoc:
    # we know about c++
    
    def __init__(self, filename, include_paths=[], clang_args=[], visitor=None, max_depth=None):
        from glob import glob

        for x in include_paths:
            for p in glob(x):
                clang_args.extend( ('-I', p) )
        index = clang.cindex.Index.create()
        # print "FNAME %s\n" % filename
        self.tu = index.parse(
            filename, 
            clang_args, 
            options= # parse everything so we can find white-space-comments
                clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
                + clang.cindex.TranslationUnit.PARSE_INCLUDE_BRIEF_COMMENTS_IN_CODE_COMPLETION
            )
        if not self.tu:
            parser.error("unable to load input")
        
        self.max_depth = max_depth
        self.filename = filename
        self.source_text = None
        self.visitor = visitor(self)
        self._visits = 0
        self.first_statement = True 

        pprint(('diags', map(get_diag_info, (d for d in self.tu.diagnostics if not re.search('__progmem__',d.spelling)))))
        # pprint(('nodes', get_info(this.tu.cursor)))
        self.visit(self.tu.cursor)

    def visit(self, node, depth=[]):
        if self.max_depth is not None and len(depth) > self.max_depth:
            print "<skip>"
            return
        if node.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION:
                pass
        elif ((node.location.file and self.filename == node.location.file.name)
                or node.kind == clang.cindex.CursorKind.TRANSLATION_UNIT):
            if node.location.file:
                print "Insert thingy"
                print "RC: %s" % node.raw_comment
                
                # first statement, there may be comments before this, and clang doesn't ast them
                if False and self.first_statement:
                    self.first_statement = False
                    with open(self.filename) as f:
                        block_start = None
                        for i,l in enumerate(f):
                            if i+1 >= node.extent.start.line:
                                break

                            if block_start:
                                line_comment = l.find('*/')
                                if line_comment != -1:
                                    was = self.source_text.insert_extent(
                                        CppDoc_Extents.Minimalist_SourceRange(
                                            CppDoc_Extents.Minimalist_SourceLocation(block_start[0]+1, block_start[1]+1),
                                            CppDoc_Extents.Minimalist_SourceLocation(i+1, line_comment+1)
                                            )
                                        )
                                    print "Pre block: %s" % was
                                    block_start = None

                                continue

                            # if re.match('\s*#',l): # pre-processor. FIXME: pick these off for #include, etc.
                            #    continue

                            line_comment = l.find('//')
                            if line_comment != -1:
                                self.source_text.insert_extent(
                                    CppDoc_Extents.Minimalist_SourceRange(
                                        CppDoc_Extents.Minimalist_SourceLocation(i+1, line_comment+1),
                                        CppDoc_Extents.Minimalist_SourceLocation(i+1, len(l))
                                        )
                                    )
                                continue;

                            line_comment = l.find('/*')
                            if line_comment != -1:
                                block_start = [i, line_comment]

                            sys.stdout.write( "PRE %s: %s" % (i+1,l))
                    
                pprint(self.visitor.describe(node,[]))
                this_extent_o = self.source_text.insert_extent(node.extent, node)
                for tok in node.get_tokens():
                    # if tok.spelling.startswith('//') or tok.spelling.startswith('/*'):
                    if tok.kind == clang.cindex.TokenKind.COMMENT:
                        # print "  T %s" % tok.spelling
                        print "Insert "+CppDoc_Visitors.Raw.describe_short(tok,False)
                        this_extent_o.insert_extent(tok.extent)
                    else:
                        # print "Insert parsed %s" % CppDoc_Visitors.Raw.describe_short(tok)
                        # this_extent_o.insert_extent(tok.extent, tok)
                        pass
            elif node.kind == clang.cindex.CursorKind.TRANSLATION_UNIT:
                self.source_text = CppDoc_Extents(node.extent, node, cppdoc=self)
                        
            sys.stdout.write("@%s" % self._visits)
            print_node_short(node, depth) if self.visitor==None else self.visitor.print_node(node, depth)
            self._visits = self._visits + 1
            if self._visits > 25:
                print("---%s-" % (self._visits-1))
                self.print_extents()
                exit(1)

            # only for method/fn decl/def/use:
            for arg_element in node.get_arguments():
                path = depth[:]
                path.append(node)
                self.visit(arg_element, path)
            for x in node.get_children():
                path = depth[:]
                path.append(node)
                self.visit(x, path)

    @property
    def file_lines(self):
        if not hasattr(self, '_file_chunks'):
            with open(self.filename) as f:
                self._file_chunks = f.readlines()
            # print "read %s lines" % len(self._file_chunks)
        return self._file_chunks

    def file_chunk(self, source_range):
        # Return a file-chunk by extent
        if len(self.file_lines) >= source_range.end.line:
            rez = []
            # print "..hunt %s:%s" % (source_range.start.line , source_range.end.line)
            # for a_line in self.file_lines[ source_range.start.line-1 : source_range.end.line ]:
            source_first = source_range.start.line-1
            source_last = source_range.end.line-1
            print "source_last %s" % source_last
            for i in range( source_first,source_last+1):
                a_line = self.file_lines[i]
                line_start = source_range.start.column-1 if i==source_first else 0
                line_end = source_range.end.column if i==source_last else len(a_line)
                print ":: %s:%s<%s> %s" % (line_start, line_end, i, a_line[ line_start:  line_end ])
                rez.append( a_line[ line_start :  line_end])
            return(rez)
        else:
            print "Noooo: have %s lines from %s, but wanted %s" % (len(self.file_lines),self.filename, source_range)

    def print_extents(self, extent=None, depth=0):
        if not extent:
            extent = self.source_text 
        print ("  " * depth) + extent.describe_short(full_token=False)
        for sub_extent in extent.internal_extent:
            self.print_extents(sub_extent, depth+1)

def range_diff(a,b):
    print "diff %s - %s" % (a,b)

from clang.cindex import SourceRange
SourceRange.__sub__ = range_diff

class CppDoc_Extents:


    class Minimalist_SourceRange:
        def __init__(self, start=None, end=None):
            self.start = start if start else CppDoc_Extents.Minimalist_SourceLocation()
            self.end = end if end else CppDoc_Extents.Minimalist_SourceLocation()

        def __repr__(self):
            return "<%s %s:%s - %s:%s>" % (self.__class__.__name__, self.start.line,self.start.column,self.end.line,self.end.column)

        def __sub__(a,b):
            print "diff %s - %s" % (a,b)
            if isinstance(b,CppDoc_Extents):
                return b-a

    class Minimalist_SourceLocation:
        def __init__(self, line=0, column=0):
            self.line=line
            self.column=column

        def __repr__(self):
            return "<%s %s:%s>" % (self.__class__.__name__, self.line,self.column)

    def __sub__(a,b):
        print "diff %s - %s" % (a,b)
        self_like = a
        if a.start > b.end:
            print "a>b"
            a,b = b,a
        elif b.end > a.end > b.start:
            print "a==b"
            return None
    
        before_line = self_like.file_lines[a.end.line-1]
        print "b>=a %s|" % before_line
        start = None
        if a.end.column >= len(before_line):
            start = CppDoc_Extents.Minimalist_SourceLocation(a.end.line+1,0)
        else:
            start = CppDoc_Extents.Minimalist_SourceLocation(a.end.line,a.end.column+1)

        end = None
        if b.start.column <= 1:
            end = CppDoc_Extents.Minimalist_SourceLocation(b.start.line - 1, len(self_like.file_lines[b.start.line-1]))
        else:
            end = CppDoc_Extents.Minimalist_SourceLocation(b.start.line, b.start.column -1)

        rez = CppDoc_Extents.Minimalist_SourceRange(start,end)
        print "a-b = %s" % rez
        return rez

    # a tree of extents, for one file, with lookup
    # has 'parsed' and 'unparsed' pieces
    # Answers given(node), what unparsed is before/after that
    def __init__(self, extent, node=None, parent=None, cppdoc=None):
        # node=None means "unparsed"
        # extents must answer to .start|end.line|column
        self.node = node
        self.extent = extent
        self.internal_extent = []
        self.parent=parent
        self.cppdoc=cppdoc
        print "  made %s" % self.extent

    def __repr__(self):
        return "<%s %s>" % (self.__class__.__name__,self.describe_extents())

    def describe_extents(self):
        return "%s:%s - %s:%s" % (self.start.line,self.start.column,self.end.line,self.end.column)

    def describe_short(self,indent=0, full_token=True):
        out = []
        out.append(self.describe_extents())
        if len(self.internal_extent) > 0:
            out.append("[%s]" % len(self.internal_extent))
        if self.node:
            if isinstance(self.node, clang.cindex.Token) and not full_token:
                out.append("token")
            out.append(CppDoc_Visitors.Raw.describe_short(self.node, with_extent=False))
        else:
            out.append('comment')
        return ("  " * indent) + " ".join(out)

    @property
    def start(self):
        return self.extent.start

    @property
    def end(self):
        return self.extent.end

    @property
    def file_lines(self):
        if self.cppdoc:
            return self.cppdoc.file_lines
        elif self.parent:
            return self.parent.file_lines
        else:
            raise Exception("No parent.cppdoc!")

    def find_comments_before(self,i):
        print "find comments before %s[%s]" % (self,i)
        print "in total lines %s" % len(self.file_lines)
        before = None
        if i != 0:
            before = self.internal_extent[i-1] 
        elif self.parent:
            before = CppDoc_Extents.Minimalist_SourceRange(
                self.start,
                self.start
                )
        else:
            CppDoc_Extents.Minimalist_SourceRange()
        after = self.internal_extent[i]

        print "find comments in %s - %s" % (before,after)
        try:
            between = before - after
        except TypeError, e:
            print "Tried %s - %s, couldn't" % (before,after)
            raise e

        print "find comments in %s" % between
        for i in range(between.start.line, between.end.line + 1):
            l = self.file_lines[i-1]
            if i == between.start.line:
                l = l[between.start.column:]
            print "  commen? %s:%s|" % (i,l)
            comment_i = l.find('//')
            if comment_i != -1:
                print "    eolcomment @%s:" % comment_i
            

    def insert_extent(self, extent, node=None):
        # returns the CppDoc_Extents inserted
        inserted = False
        #print "Insert %s" % extent
        for i,current_extent in enumerate(self.internal_extent):
            # find the internal it is in,before or after, overlaps
            # print "  %s ?? %s" % (i, current_extent)
            
            es=extent.start
            ee=extent.end
            cs=current_extent.start
            ce=current_extent.end

            # before
            if es.line < cs.line or (es.line == cs.line and es.column < cs.column):
                #print "  Before"
                new_e = CppDoc_Extents(extent,node,self)
                self.internal_extent.insert(i, new_e)
                self.find_comments_before(i)
                inserted = True
                break

            # overlap|in
            elif cs.line <= es.line <= ce.line:
                #print "  in oneline %s" % ce
                # one line
                if ce.line == cs.line:
                    #print "vs one line, [%s:%s] in [%s:%s]" % (es.column, ee.column,cs.column, ce.column)
                    if inside_range([es.column, ee.column], [cs.column, ce.column]):
                        #print "Inside %s" % ce
                        new_e = current_extent.insert_extent(extent, node)
                        inserted = True
                        break
                    elif inside_range(es.column, [cs.column, ce.column]) or inside_range(ee.column, [cs.column, ce.column]):
                        # raise Exception("Overlap at %s" % extent)
                        sys.stderr.write("Overlap at %s\n" % extent)

                # multiline
                else:
                    starts_in = False
                    # on first line
                    if es.line == cs.line:
                        starts_in = es.column >= cs.column
                    # on last line
                    elif es.line == ce.line:
                        starts_in = es.column <= ce.column
                    else:
                        starts_in = True

                    ends_in = False
                    # on first line
                    if ee.line == cs.line:
                        ends_in = ee.column >= cs.column
                    elif ee.line == ce.line:
                        ends_in = ee.column <= ce.column
                    else:
                        ends_in = True

                    if starts_in and ends_in:
                        #print "  Inside (m) %s" % current_extent
                        new_e = current_extent.insert_extent(extent, node)
                        inserted = True
                        break
                    elif starts_in or ends_in:
                        raise Exception("Overlap at %s" % extent)

        if not inserted:
            #print "  After %s" % (self.internal_extent[-1] if len(self.internal_extent)>0 else '[]')
            new_e = CppDoc_Extents(extent,node,self)
            self.internal_extent.append(new_e)
        #print "--done"
        return new_e

class CppDoc_Empty:
    def __init__():
        pass

class CppDoc_Visitors:
    class DeclVisitor:
        def __init__(self,translationUnit):
            self.tu = translationUnit

        def print_node(self,node,depth):
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

        def x_standard_methods(self):
            if not hasattr(self,'_standard_methods'):
                self._standard_methods = CppDoc_Empty.__dict__.keys()
            return self._standard_methods

        @classmethod
        def describe_extent(self, node):
            return("%s:%s - %s:%s" % (node.extent.start.line,node.extent.start.column,node.extent.end.line,node.extent.end.column))

        @classmethod
        def describe_name_short(self,node):
            if hasattr(node,'displayname') and node.displayname:
                return(node.displayname)
            elif hasattr(node,'spelling') and node.spelling:
                return(node.spelling)
            elif hasattr(node,'name') and node.name:
                return(node.name)

        @classmethod
        def describe_short(self, node, with_extent=True):
            out = []
            if with_extent:
                out.append(self.describe_extent(node))

            # e.g. CursorKind.TRANSLATION_UNIT
            # print "try: %s" % node.kind
            ckind= re.match('(CursorKind|TokenKind)\.(.+)', str(node.kind)).group(2)
            out.append(ckind)

            if hasattr(node,'result_type') and node.result_type and node.result_type.kind != clang.cindex.TypeKind.INVALID:
                # gives a Class Type
                out.append(self.describe_type_short(node.result_type))

            elif hasattr(node,'type') and node.type and node.type.kind != clang.cindex.TypeKind.INVALID :
                out.append( self.describe_type_short(node.type))

            name = self.describe_name_short(node)
            if name:
                out.append(name)
                
            return " ".join(out)
            
        @classmethod
        def describe(self,node,depth):
            out = []
            out.append(self.describe_extent(node))
            name = []
            if hasattr(node,'displayname') and node.displayname:
                name.append(node.displayname)
            if hasattr(node,'spelling') and node.spelling:
                name.append('spelling:%s' % node.spelling)
            if len(name) > 0:
                out.append(name)

            # We never get either of these
            if hasattr(node,'brief_comment') and node.brief_comment:
                out.append("// " + brief_comment)
            if hasattr(node,'raw_comment') and node.raw_comment:
                out.append(["raw_comment",raw_comment])

            # get_arguments is an iter of cursor
            args = []
            for arg_element in node.get_arguments():
                # args.append("%s" % self.describe(arg_element,depth))
                args.append(self.describe(arg_element,depth))
            if len(args) > 0:
                out.append(["(",args,")"])

            type = []
            if hasattr(node,'result_type') and node.result_type and node.result_type.kind != clang.cindex.TypeKind.INVALID:
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
                # seems redundant
                # if depth != -1:
                #    type.append("%s" % self.describe(node.get_definition(),-1))
            if node.kind.is_attribute():
                type.append('attr')
            if node.kind.is_preprocessing():
                type.append('preproc')
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
            if node.kind.is_statement():
                general_kind.append('stmt')
            # more general_kind
            out.append([node.kind, " ".join(general_kind)])
            # out.append( ['methods' ," ".join(sorted(list(x for x in node.__class__.__dict__.keys() if not (x in self.x_standard_methods()))))])
            return out

        @classmethod
        def describe_type_short(self,node):
            out = []
            if node.is_const_qualified():
                out.append('const')
            if node.is_volatile_qualified():
                out.append('volatile')
            if node.is_restrict_qualified():
                out.append('restrict')
            if hasattr(node,'get_ref_qualifier') and node.get_ref_qualifier() and node.get_ref_qualifier() != clang.cindex.RefQualifierKind.NONE:
                out.append("%s" % node.get_ref_qualifier())
            if node.get_pointee() and node.get_pointee().kind != clang.cindex.TypeKind.INVALID:
                out.extend(["(*", self.describe_type_short(node.get_pointee()),")"])
            if node.get_array_element_type() and node.get_array_element_type().kind != clang.cindex.TypeKind.INVALID:
               out.extend(['[%s]' % node.get_array_size(), self.describe_type_short(node.get_array_element_type())])
            out.append(self.describe_name_short(node))
            return " ".join(out)

        @classmethod
        def describe_type(self,node):
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
            if hasattr(node,'get_ref_qualifier') and node.get_ref_qualifier() and node.get_ref_qualifier() != clang.cindex.RefQualifierKind.NONE:
                out.append(['ref_qualifier', node.get_ref_qualifier()])
            if node.get_pointee() and node.get_pointee().kind != clang.cindex.TypeKind.INVALID:
                out.append(["*", self.describe_type(node.get_pointee())])
            if node.get_array_element_type() and node.get_array_element_type().kind != clang.cindex.TypeKind.INVALID:
                out.append(['[%s]' % node.get_array_size(), self.describe_type(node.get_array_element_type())])
            if hasattr(node,'get_class_type') and node.get_class_type():
                out.append(['class_type', node.get_class_type()])

            return out

        def print_node(self,node,depth):
            pprint(self.describe(node,depth))




def main():
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
    print "---"
    parsed.print_extents()

if __name__ == '__main__':
    main()
EOF
