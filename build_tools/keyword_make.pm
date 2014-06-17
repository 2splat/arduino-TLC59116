#!/usr/bin/env perl
use strict; use warnings; no warnings 'uninitialized'; use 5.010;

# -CXXRecordDecl 0xa343350 <build/TLC59116.hpp:36:1, line:291:1> class TLC59116
our $typical_line = qr/(\||`)-(\w+)Decl 0x[0-9a-f]+ <[^>]+> (.+)/;
our ($type,$what);
our $access;
our %seen;

if ($_ =~ $typical_line) {
    $type = $2 . "Decl";
    $what = $3;
    }
else {
    ($type, $what) = (undef,);
    }

$type eq 'AccessSpecDecl' && do {
    $access = $what;
    next;
    };

next if $access eq 'private';

$type eq 'CXXRecordDecl' && !/, col:\d/ && $what =~ /^class (\w+)$/ && do {
    my $class = $1;
    next if $seen{"class $1"};
    $seen{"class $1"} = 1;
    say "$class KEYWORD1";
    next;
    };

$type eq 'CXXMethodDecl' && $what =~ /^(\w+)/ && do {
    next if $what =~ /^_/;
    next if $seen{"method $1"};
    $seen{"method $1"} = 1;
    say "$1 KEYWORD2";
    next;
    };

$type eq 'VarDecl' && $what =~ /'const / && $what =~ / static$/ && $what =~ /^(\w+)/ && do {
    next if $what =~ /^_/;
    next if $seen{"var $1"};
    $seen{"var $1"} = 1;
    say "$1 KEYWORD2";
    next;
    };
