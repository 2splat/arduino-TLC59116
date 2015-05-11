from cppdoc_processor import CppDoc_Processor
class CppDoc_Processor_WikiProcessor(CppDoc_Processor):
    def __call__(self):
        print "WP!"
        for p in self.parsed:
            # p.print_extents()
            for e in p.source_text:
                if not e.node:
                    print "%s: %s" % (e.__class__.__name__,e.describe_short)

