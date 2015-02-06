#!/usr/bin/env python
# env LD_LIBRARY_PATH=`llvm-config-3.4 --libdir` python - "$@" <<EOF

"""
A simple command line tool for dumping a source file using the Clang Index
Library.
"""
import re
import sys
import inspect # inspect.currentframe().f_lineno
import clang
    # https://github.com/llvm-mirror/clang/blob/release_34/bindings/python/clang/cindex.py
from pprint import pprint,pformat
from clang.cindex import SourceRange,Cursor,Token

# not a class/nor instance method
def inside_range(a,b):
    # a in-the-range [x,y]
    # a can also be [j,k]: a overlaps b
    # print "test %s in %s" % (a,b)
    if isinstance(a,list):
        return (b[0] <= a[0] <= b[1]) and (b[0] <= a[1] <= b[1])
    else:
        return b[0] >= a >= b[1]

def _describe_extent(self):
    return "%s:%s - %s:%s" % (self.start.line,self.start.column,self.end.line,self.end.column)
class _extents_extensions:
    pass
_extents_extensions.describe = _describe_extent # because "unbound..." in SourceRange monkey-patch below
SourceRange.describe = _describe_extent

# OMG, c++ foreign classes: can't make one ourselves, and it won't do '==' with our pseudo class.
def sreq(a,b):
    return ( a.start.line == b.start.line and a.start.column == b.start.column
    and a.end.line == b.end.line and a.end.column == b.end.column)
SourceRange.__eq__ = sreq
    
def _describe_node(self,depth=None):
    out = []
    if hasattr(self,'extent'):
        out.append(self.extent.describe())
    name = []
    if hasattr(self,'displayname') and self.displayname:
        name.append(self.displayname)
    if hasattr(self,'spelling') and self.spelling:
        name.append('spelling:%s' % self.spelling)
    if len(name) > 0:
        out.append(name)

    # We never get either of these
    if hasattr(self,'brief_comment') and self.brief_comment:
        out.append("// " + brief_comment)
    if hasattr(self,'raw_comment') and self.raw_comment:
        out.append(["raw_comment",raw_comment])

    # get_arguments is an iter of cursor
    args = []
    if hasattr(self,'get_arguments'):
        for arg_element in self.get_arguments():
            args.append(arg_element.describe(depth))
        if len(args) > 0:
            out.append(["(",args,")"])

    if hasattr(self,'result_type') and self.result_type and self.result_type.kind != clang.cindex.TypeKind.INVALID:
        # gives a Class Type
        out.append(['result_type',self.result_type.describe()])

    type = []
    if hasattr(self,'access_specifier') and self.access_specifier:
        type.append("access_specifier:%s" % self.access_specifier)
    if hasattr(self,'is_static_method') and self.is_static_method():
        type.append('static')
    if hasattr(self,'storage_class') and self.storage_class:
        type.append("storage_class:%s" % storage_class)
    if hasattr(self,'is_definition') and self.is_definition():
        type.append('defn')
        # seems redundant
    if hasattr(self.kind,'is_attribute') and self.kind.is_attribute():
        type.append('attr')
    if hasattr(self.kind,'is_preprocessing') and self.kind.is_preprocessing():
        type.append('preproc')
    if self.kind == clang.cindex.CursorKind.ENUM_DECL:
        # a Class Type...
        type.append("enum_type=%s",self.enum_type)
    if self.kind == clang.cindex.CursorKind.ENUM_CONSTANT_DECL:
        type.append("enum_value=%s",self.enum_value)
    if hasattr(self,'is_bitfield') and self.is_bitfield():
        type.append("bitfield[%s]" % self.get_bitfield_width())
    if len(type)>0:
        out.append(" ".join(type))
    if depth != -1 and hasattr(self,'type') and self.type and self.type.kind != clang.cindex.TypeKind.INVALID :
        # out.append( self.type.describe())
        out.append(self.type.describe())

    # get_definition gives a cursor if is_definition?
    general_kind=[]
    if hasattr(self.kind,'is_reference'):
        if self.kind.is_reference():
            general_kind.append('ref')
        if self.kind.is_expression():
            general_kind.append('expr')
        if self.kind.is_declaration():
            general_kind.append('decl')
        if self.kind.is_statement():
            general_kind.append('stmt')
        if self.kind.is_statement():
            general_kind.append('stmt')
    # more general_kind
    out.append([self.kind, " ".join(general_kind)])

    return out

def _describe_type_short(self):
    out = []
    if self.is_const_qualified():
        out.append('const')
    if self.is_volatile_qualified():
        out.append('volatile')
    if self.is_restrict_qualified():
        out.append('restrict')
    if hasattr(self,'get_ref_qualifier') and self.get_ref_qualifier() and self.get_ref_qualifier() != clang.cindex.RefQualifierKind.NONE:
        out.append("%s" % self.get_ref_qualifier())
    if self.get_pointee() and self.get_pointee().kind != clang.cindex.TypeKind.INVALID:
        out.extend(["(*", self.get_pointee().describe_short(),")"])
    if self.get_array_element_type() and self.get_array_element_type().kind != clang.cindex.TypeKind.INVALID:
       out.extend(['[%s]' % self.get_array_size(), self.get_array_element_type().describe_short()])
    out.append(self.describe_name_short())
    return " ".join(out)

def _describe_type(self):
    out = []
    out.append(self.kind)
    if self.get_declaration() and self.get_declaration().displayname:
        out.append(self.get_declaration().displayname)
    if hasattr(self,'spelling') and self.spelling:
        out.append("SP:"+self.spelling)
    # out.append(self.element_type)
    if self.is_const_qualified():
        out.append('const')
    if self.is_volatile_qualified():
        out.append('volatile')
    if self.is_restrict_qualified():
        out.append('restrict')
    if hasattr(self,'get_ref_qualifier') and self.get_ref_qualifier() and self.get_ref_qualifier() != clang.cindex.RefQualifierKind.NONE:
        out.append(['ref_qualifier', self.get_ref_qualifier()])
    if self.get_pointee() and self.get_pointee().kind != clang.cindex.TypeKind.INVALID:
        out.append(["*", self.get_pointee().describe()])
    if self.get_array_element_type() and self.get_array_element_type().kind != clang.cindex.TypeKind.INVALID:
        out.append(['[%s]' % self.get_array_size(), self.get_array_element_type().describe()])
    if hasattr(self,'get_class_type') and self.get_class_type():
        out.append(['class_type', self.get_class_type()])

    return out

def _describe_name_short(self):
    if hasattr(self,'displayname') and self.displayname:
        return(self.displayname)
    elif hasattr(self,'spelling') and self.spelling:
        return(self.spelling)
    elif hasattr(self,'name') and self.name:
        return(self.name)

def _describe_short(self, with_extent=True):
    out = []
    # if len(out)> 0: print "### %s for [%s] @ %s" % (out[-1].__class__.__name__,len(out),inspect.currentframe().f_lineno)
    if with_extent:
        out.append(self.extent.describe())
    # if len(out)> 0: print "### %s for [%s] @ %s" % (out[-1].__class__.__name__,len(out),inspect.currentframe().f_lineno)

    # e.g. CursorKind.TRANSLATION_UNIT
    # print "try: %s" % self.kind
    ckind= re.match('(CursorKind|TokenKind)\.(.+)', str(self.kind)).group(2)
    out.append(ckind)
    # print "### %s for [%s] @ %s" % (out[-1].__class__.__name__,len(out),inspect.currentframe().f_lineno)

    if hasattr(self,'result_type') and self.result_type and self.result_type.kind != clang.cindex.TypeKind.INVALID:
        # gives a Class Type
        out.append(self.result_type.describe_short())
        # print "### %s for [%s] @ %s" % (out[-1].__class__.__name__,len(out),inspect.currentframe().f_lineno)

    elif hasattr(self,'type') and self.type and self.type.kind != clang.cindex.TypeKind.INVALID :
        out.append( "%s" % self.type.describe_short())
        # print "### %s for [%s] @ %s" % (out[-1].__class__.__name__,len(out),inspect.currentframe().f_lineno)

    name = self.describe_name_short()
    if name:
        out.append(name)
    # print "### %s for [%s] @ %s" % (out[-1].__class__.__name__,len(out),inspect.currentframe().f_lineno)
        
    for i,x in enumerate(out):
        if isinstance(x,list):
            # print "#### %s" % i
            pprint(x)
    return " ".join(out)
    
def _print_node(self,depth):
    pprint(self.describe(depth))

class _node_extensions:
    pass
_node_extensions.describe=_describe_node
_node_extensions.describe_short=_describe_short
_node_extensions.describe_type=_describe_type
_node_extensions.describe_type_short=_describe_type_short
_node_extensions.describe_name_short=_describe_name_short
_node_extensions.print_node=_print_node
Cursor.describe=_describe_node
Cursor.describe_short=_describe_short
Cursor.describe_type=_describe_type
Cursor.describe_type_short=_describe_type_short
Cursor.describe_name_short=_describe_name_short
Cursor.print_node=_print_node
Token.describe_short=_describe_short
Token.describe_name_short=_describe_name_short
clang.cindex.Type.describe=_describe_type
clang.cindex.Type.describe_short=_describe_type_short
clang.cindex.Type.describe_name_short=_describe_name_short


import os

if os.environ.get('DEBUG'):
    def debug(msg):
        print msg
else:
    def debug(msg):
        pass

class CppDoc:
    # we know about c++

    def __init__(self, filename, include_paths=[], clang_args=[], max_depth=None, debug_limit=None):
        from glob import glob

        for x in include_paths:
            for p in glob(x):
                clang_args.extend( ('-I', p) )
        index = clang.cindex.Index.create()
        debug( "FNAME %s\n" % filename )
        self.tu = index.parse(
            filename, 
            clang_args, 
            options= # parse everything so we can find white-space-comments
                clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
                + clang.cindex.TranslationUnit.PARSE_INCLUDE_BRIEF_COMMENTS_IN_CODE_COMPLETION
            )
        if not self.tu:
            raise Exception("unable to load input")
            
        self.max_depth = max_depth
        self.filename = filename
        self.source_text = None
        self._visits = 0
        self.first_statement = True 
        self.debug_limit = debug_limit

        # for d in ([d for d in self.tu.diagnostics if not re.search('__progmem__',d.spelling]):
        for diag in self.tu.diagnostics:
            if re.search('__progmem__',diag.spelling):
                next
            sys.stderr.write(pformat( [ diag.spelling, diag.location, "severity:%s" % diag.severity ]))
            sys.stderr.write("\n")

        self.visit(self.tu.cursor)
        debug("---REVISIT");
        self.revisit_for_toplevel_comments()

    def visit(self, node, depth=[]):
        # Not actually "visit", but "build ast"
        # NB: a node may be visited more than once.
        # Specifically, comments (seen as node.getTokens()) will be seen
        #       for a class (all internal comments)
        #       then again for a declaration
        #       and again for elements of the declaration
        # Further, there seems to be a bug where a macro use will give
        #       the token stream starting at the #define!
        #       Each time!
        #       Thus, you'll see the comments n times
        if self.debug_limit and self._visits > self.debug_limit:
            debug( "Debug limit %s reached" % self.debug_limit)
            return
        if self.max_depth is not None and len(depth) > self.max_depth:
            debug( "<skip>")
            return
        if node.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION:
                pass
        elif ((node.location.file and self.filename == node.location.file.name)
                or node.kind == clang.cindex.CursorKind.TRANSLATION_UNIT):
            self._visits = self._visits + 1
            if node.location.file:
                debug( "Insert thingy")
                # print "RC: %s" % node.raw_comment # we never see a raw_comment, whatever that is
                    
                debug(pformat(node.describe()))
                this_extent_o = self.source_text.insert_extent(node.extent, node)
                for tok in node.get_tokens():
                    if tok.kind == clang.cindex.TokenKind.COMMENT:
                        # print "  T %s" % tok.spelling
                        outline = "@%s['%s'] %s" % (
                            self._visits, 
                            tok.extent.describe(),
                            tok.describe_short(False)
                            )
                        debug(outline.rstrip())
                        # this_extent_o.insert_extent(tok.extent)
                        self.insert_toplevel_node(tok)
                        # sys.stdout.write("-comment inserted")
                    else:
                        # print "Insert parsed %s" % tok.describe_short()
                        # this_extent_o.insert_extent(tok.extent, tok)
                        pass
            elif node.kind == clang.cindex.CursorKind.TRANSLATION_UNIT:
                self.source_text = CppDoc_Extents(node.extent, node, cppdoc=self)
                        
            if os.environ.get('DEBUG'):
                sys.stdout.write("@%s" % self._visits)
                node.print_node(depth)

            # only for method/fn decl/def/use:
            for arg_element in node.get_arguments():
                path = depth[:]
                path.append(node)
                self.visit(arg_element, path)
            for x in node.get_children():
                path = depth[:]
                path.append(node)
                self.visit(x, path)

            if node.kind == clang.cindex.CursorKind.TRANSLATION_UNIT:
                debug( "END TU")

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
            # print "source_last %s" % source_last
            for i in range( source_first,source_last+1):
                a_line = self.file_lines[i]
                line_start = source_range.start.column-1 if i==source_first else 0
                line_end = source_range.end.column if i==source_last else len(a_line)
                # print ":: %s:%s<%s> %s" % (line_start, line_end, i, a_line[ line_start:  line_end ])
                rez.append( a_line[ line_start :  line_end])
            return(rez)
        else:
            raise Exception(
                "Noooo: have %s lines from %s, but wanted %s" % (len(self.file_lines),self.filename, source_range)
                )

    def revisit_for_toplevel_comments(self):
        # only for top level
        debug( self.source_text.describe_short(full_token=False))
        
        prev_end = CppDoc_Extents.Minimalist_SourceLocation(
            self.source_text.start.line,
            self.source_text.start.column-1
            )
        to_insert = []
        for si, sub_extent in enumerate(self.next_used_chunk(self.source_text)): # was sub_extent
            debug( "  <%s> %s" % (si, sub_extent.describe()))
            if CppDoc_Extents.Minimalist_SourceLocation.__gt__(sub_extent.start, prev_end):
                gap = CppDoc_Extents.Minimalist_SourceRange(
                    CppDoc_Extents.Minimalist_SourceLocation(prev_end.line, prev_end.column+1),
                    CppDoc_Extents.Minimalist_SourceLocation(sub_extent.start.line, sub_extent.start.column-1)
                    )
                debug( "    gap %s" % gap)
                gap_lines = self.file_chunk(gap)
                # for l in gap_lines:
                #     sys.stdout.write("::")
                #     sys.stdout.write(l)
                for c in self.comments_in_lines(gap.start,gap_lines):
                    # print "C:",c.spelling # describe
                    debug( "[%s] %s" % (
                            c.describe_extents(),
                            c.describe_short(False)
                            ))
                    to_insert.append(c)
            prev_end = sub_extent.end
        for c in to_insert:
            self.insert_toplevel_node(c)
        debug("<end revisit>")

    def next_used_chunk(self, extent):
        debug( "used for [%s]..." % extent.describe_extents())
        for n in extent.internal_extent:
            debug( "  used [%s], has %s[]" % (n.describe_extents(), len(n.internal_extent)))
            yield n.extent # top level...
            if len(n.internal_extent) > 0:
                debug( "    0th is %s" % n.internal_extent[0].describe_extents())
            if (
                len(n.internal_extent) > 0 
                and CppDoc_Extents.Minimalist_SourceLocation.__gt__(
                    n.internal_extent[0].start,n.end
                )):
                debug( "    not encompassed in used: [%s]" % n.internal_extent[0].describe_extents())
                for x in self.next_used_chunk(n):
                    yield x

    def insert_toplevel_node(self,node):
        inserted = False
        for i,tn in enumerate(self.source_text.internal_extent):
            if CppDoc_Extents.Minimalist_SourceLocation.__gt__(tn.start, node.location):
                # print tn.node.describe_short()
                # print "insert " + node.extent.describe()
                # print "before [%s] %s" % (i, tn.node.describe_short())
                # print "Make ccpde w %s %s" % (tn.extent,tn)
                new_e = CppDoc_Extents(node.extent,None,self)
                self.source_text.internal_extent.insert(i,new_e)
                inserted = True
                break
            elif tn.extent == node.extent:
                # print "Duplicate comment %s" % tn
                inserted = True
                break

        if not inserted:
            new_e = CppDoc_Extents(node.extent,None,self)
            self.source_text.internal_extent.append(new_e)

    def comments_in_lines(self, start_location, lines):
        # generator returning comments
        comment_node = None # in block (multi-line) comment?

        for i,l in enumerate(lines):
            start_offset = start_location.column - 1 if i==0 else 0
            if comment_node: # in /*...*/
                block_end = re.search('\*/',l)
                if block_end:
                    comment_node.spelling.append( l[start_offset:block_end.end()+start_offset] )
                    comment_node.extent.end = CppDoc_Extents.Minimalist_SourceLocation(
                        start_location.line + i, block_end.end() + start_offset
                        )
                    comment_node.spelling = "".join(comment_node.spelling)

                    yield comment_node

                    # print "<endblock>"
                    comment_node = None

                    # fall through, might have // at end!
                    start_offset = block_end.end()+start_offset
                    l = l[start_offset+1:]
                else:
                    comment_node.spelling.append(l)
                    continue

            linecomment = re.search('//',l)
            if linecomment:
                lc_extent = CppDoc_Extents.Minimalist_SourceRange.from_line_columns(
                        start_location.line + i, start_offset + linecomment.start()+1,
                        start_location.line + i, len(l)-1 + start_offset +1
                        )
                yield CppDoc.Comment_Node(
                    spelling = l[linecomment.start():],
                    extent = lc_extent,
                    location = lc_extent.start,
                    )
                continue

            block_start = re.search('/\*',l)
            if block_start:
                # print "<blockstart>"
                block_end = re.search('\*/',l)
                bend = block_end.end() if block_end else len(l)

                extent = CppDoc_Extents.Minimalist_SourceRange.from_line_columns(
                        start_location.line + i, block_start.start() + start_offset + 1,
                        0,0
                        )
                comment_node = CppDoc.Comment_Node(
                    spelling = [ l[block_start.start():bend] ], # list of strings, join later
                    extent = extent,
                    location = extent.start,
                    )
                if block_end:
                    # 1 line
                    comment_node.extent.end = CppDoc_Extents.Minimalist_SourceLocation(
                        start_location.line + i, bend + start_offset +1
                        )
                    comment_node.spelling = "".join(comment_node.spelling)
                    yield comment_node
                    # print "<shortblock>"
                    comment_node = None
                continue

    def print_extents(self, extent=None, depth=0):
        raise Exception("Bob")
        if not extent:
            extent = self.source_text 
        print ("  " * depth) + extent.describe_short(full_token=False)
        for sub_extent in extent.internal_extent:
            self.print_extents(sub_extent, depth+1)

    class Comment_Node(_node_extensions):
        def __init__(self, location, spelling, extent):
            self.location = location
            self.extent = extent
            self.spelling = spelling

        def describe_extents(self):
            return self.extent.describe()

        @property
        def kind(self):
            return clang.cindex.TokenKind.COMMENT



class CppDoc_Extents:

    class Minimalist_SourceRange(_extents_extensions):
        @classmethod
        def from_line_columns(self, start_line, start_column, end_line, end_column):
            return CppDoc_Extents.Minimalist_SourceRange(
                CppDoc_Extents.Minimalist_SourceLocation(start_line, start_column),
                CppDoc_Extents.Minimalist_SourceLocation(end_line, end_column)
                )

        def __init__(self, start=None, end=None):
            self.start = start if start else CppDoc_Extents.Minimalist_SourceLocation()
            self.end = end if end else CppDoc_Extents.Minimalist_SourceLocation()

        def __repr__(self):
            return "<%s %s:%s - %s:%s>" % (self.__class__.__name__, self.start.line,self.start.column,self.end.line,self.end.column)

        def __sub__(a,b):
            # print "diff %s - %s" % (a,b)
            if isinstance(b,CppDoc_Extents):
                return b-a

    class Minimalist_SourceLocation:
        def __init__(self, line=0, column=0):
            self.line=line
            self.column=column

        def __repr__(self):
            return "<%s %s:%s>" % (self.__class__.__name__, self.line,self.column)

        @classmethod
        def __gt__(self,a,b):
            # print "gt: %s (%s) > %s (%s) ? %s" % (a, a.line.__class__.__name__, b, b.line.__class__.__name__, a.line>b.line)
            return a.line > b.line or (a.line==b.line and a.column > b.column)

    def __sub__(a,b):
        # print "diff %s - %s" % (a,b)
        self_like = a
        if a.start > b.end:
            # print "a>b"
            a,b = b,a
        elif b.end > a.end > b.start:
            # print "a==b"
            return None
    
        before_line = self_like.file_lines[a.end.line-1]
        # print "b>=a %s|" % before_line
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
        # print "a-b = %s" % rez
        return rez

    # a tree of extents, for one file, with lookup
    # has 'parsed' and 'unparsed' pieces
    def __init__(self, extent, node=None, parent=None, cppdoc=None):
        # node=None means "unparsed"
        # extents must answer to .start|end.line|column
        self.node = node
        self.extent = extent
        self.internal_extent = []
        self.parent=parent
        self.cppdoc=cppdoc
        #print "  made %s" % self.extent

    def __repr__(self):
        return "<%s %s>" % (self.__class__.__name__,self.describe_extents())

    def describe_extents(self):
        return self.extent.describe()

    def describe_short(self,indent=0, full_token=True):
        out = []
        out.append(self.describe_extents())
        if len(self.internal_extent) > 0:
            out.append("[%s]" % len(self.internal_extent))
        if self.node:
            if isinstance(self.node, clang.cindex.Token) and not full_token:
                out.append("token")
            out.append(self.node.describe_short(False))
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

    def insert_extent(self, extent, node=None):
        # returns the CppDoc_Extents inserted
        inserted = False
        # print "Insert %s Node %s" % (extent,node)
        for i,current_extent in enumerate(self.internal_extent):
            # find the internal it is in,before or after, overlaps
            # print "  %s ?? %s" % (i, current_extent)
            
            es=extent.start
            ee=extent.end
            cs=current_extent.start
            ce=current_extent.end

            # before
            if es.line < cs.line or (es.line == cs.line and es.column < cs.column):
                # print "  Before %s" % current_extent
                new_e = CppDoc_Extents(extent,node,self)
                self.internal_extent.insert(i, new_e)
                inserted = True
                break

            # overlap|in
            elif cs.line <= es.line <= ce.line:
                #print "  in oneline %s" % ce
                # one line
                if ce.line == cs.line:
                    #print "vs one line, [%s:%s] in [%s:%s]" % (es.column, ee.column,cs.column, ce.column)
                    if not node and cs.line == es.line and cs.column == es.column and ce.column == ee.column:
                        # print("Duplicate comment at %s" % extent)
                        new_e = current_extent
                        inserted = True # discarded, actually
                        break
                    elif inside_range([es.column, ee.column], [cs.column, ce.column]):
                        # print "Inside %s" % current_extent
                        new_e = current_extent.insert_extent(extent, node)
                        inserted = True
                        break
                    elif inside_range(es.column, [cs.column, ce.column]) or inside_range(ee.column, [cs.column, ce.column]):
                        # raise Exception("Overlap at %s" % extent)
                        sys.stderr.write("Overlap at %s\n" % extent)

                # multiline
                else:
                    if not node and es == cs:
                        inserted = True
                        new_e = current_extent
                        # print("Duplicate (m) comment at %s" % extent)
                        break

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
                        # print "  Inside (m) %s" % current_extent
                        new_e = current_extent.insert_extent(extent, node)
                        inserted = True
                        break
                    elif starts_in or ends_in:
                        sys.stderr.write("Warning: Overlap at %s\nn" % extent)

        if not inserted:
            # print "  After %s" % (self.internal_extent[-1] if len(self.internal_extent)>0 else '[]')
            new_e = CppDoc_Extents(extent,node,self)
            self.internal_extent.append(new_e)
        # print "--done at %s" % new_e
        return new_e

