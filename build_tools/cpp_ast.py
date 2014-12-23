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
    return { 'severity' : diag.severity,
             'location' : diag.location,
             'spelling' : diag.spelling,
             'ranges' : diag.ranges,
             'fixits' : diag.fixits }

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
    pprint(('path', path))
    pprint({ 
             '*' : len(depth),
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
    
    def __init__(self, filename, include_paths=[], clang_args=[], max_depth=None):
        from clang.cindex import Index, TranslationUnit
        from glob import glob

        for x in include_paths:
            for p in glob(x):
                clang_args.extend( ('-I', p) )
        index = Index.create()
        print "FNAME %s\n" % filename
        self.tu = index.parse(filename, clang_args) # parse everything so we can find white-space-comments
        if not self.tu:
            parser.error("unable to load input")
        
        self.max_depth = max_depth
        self.filename = filename

        # pprint(('diags', map(get_diag_info, self.tu.diagnostics)))
        # pprint(('nodes', get_info(this.tu.cursor)))
        self.visit(self.tu.cursor)

    def visit(self, node, depth=[]):
        if opts.maxDepth is not None and len(depth) > opts.maxDepth:
            print "<skip>"
            return
        elif node.location and node.location.file and re.search('TLC59116.hpp',node.location.file.name):
            print "Loc %s" % node.location
            print_node(node, depth)

        for x in node.get_children():
            path = depth[:]
            path.append(node)
            self.visit(x, path)

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
    parser.disable_interspersed_args()
    (opts, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('Expected input-file-name')

    pprint(('argv', args))
    parsed = CppDoc(args[0], opts.includePaths.split(','))

if __name__ == '__main__':
    main()
EOF
