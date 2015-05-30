#!/usr/bin/env perl
# --- # test/doc output
# --- template-file-source [k=v...] > stdout
# OR
# use cutnpaste_template;
# render("some template...", {some => $data...});
#
# See __DATA_ section for syntax of template
# version 2: supports escaping the sigils
# version 3: supports catenation, $_ in iteration, and $0..$9 for arrays

use strict; use warnings; no warnings 'uninitialized';
use Text::Balanced qw(extract_bracketed);
sub render {
    my ($template, $data) = @_;

    my @rez;
    # 1st iteration: head@name[block]rest...
    # $template = "rest" for next split
    # Split gives just "head" if no "@", so processes each piece
    # avoid $' with a split
    # We actually split on \@ or @, so we can remove the \
    while (my ($head, $escape, $field_name, $block) = split(/(\\?)@([a-zA-Z]\w*|_)(?=\[)/, $template,2)) {
        # this was a \@word[..., so remove the \
        if ($escape) {
          $head = $head."@".$field_name;
          }

        # fix bracket escapes
        $head =~ s/\\\[/[/g;
        $head =~ s/\\\]/]/g;

        # interpolate scalars in the "head"
        # Have to capture the \ so we can do the right thing
        my $interp = sub {
          # escape, fieldname
          if ($1) { '$'.$2 } # escaped, so remove /
          elsif (ref($data->{'_'}) eq 'ARRAY' && index('0123456789',$2) >= 0) { 
            $data->{'_'}->[$2]; 
            }
          else { $data->{$2}; } # interp
          };
        $head =~ s/(\\?)\$([a-zA-Z]\w*|_|[0-9])/&$interp/eg;
        push @rez, $head;

        last if ! $field_name;

        # this was a \@word[..., so next on ...
        if ($field_name =~ /^\\(.+)/) {
          $template = $block;
          next;
          }

        # Get the "block" and "rest"
        my $bracketed;
        ($bracketed, $template) = extract_bracketed( $block, '[');
        $bracketed =~ s/^\[//;
        $bracketed =~ s/\]$//;

        # Repeat the block
        my $list = (ref($data->{$field_name}) eq 'ARRAY') ? $data->{$field_name} : [$data->{$field_name}];
        foreach my $sub_data ( @$list ) {
            # recurse on this block with our block's data
            push @rez, render( $bracketed, {%$data, (ref($sub_data) eq 'HASH' ? %$sub_data : ()), '_' => $sub_data});
            }
        }
    join("",@rez);
    }

package cutnpaste_template_runner;

use strict; use warnings; no warnings 'uninitialized';

use File::Basename;
use IO::File;
use Data::Dumper;

sub use_test_data {
  my ($data) = $_;
  $data->{'file'} = (-e $ARGV[0]) ? basename($ARGV) : "__DATA__ section";
  $data->{'name'} = "'Toplevel ".$data->{'file'}."'";
  $data->{'has_dollar_in_value'}='<this interpolation has a dollar sign: $not_reinterpolated>';
  $data->{'outer'} = [
    { name => 'block 1', outerValue => 'block1-outer',
      inner => [{ name => 'inner1.1'},{ name => 'inner1.2'}],
      },
    { name => 'block 2', outerValue => 'block2-outer',
      inner => [{ name => 'inner2.1'},{ name => 'inner2.2'}],
      }
    ];
  $data->{'lol'} = [ # list-of-lists
    [qw(1 2 3)],
    [qw(a b c $no_recursive_interpolation_of_data)],
    ];
  }

sub main {
    if ($ARGV[0] eq '-linecount') {
      # without comments and blank lines
      exec q{awk '/^use Text::Balanced/,/^package/' < "}.$0.q{" | awk '/[ \t]*#/ || /^[ \t]*$/ || /^package/ {next} ; {print}' | wc -l};     
    }

    my %data;

    # data file
    %data = do(shift @ARGV) if $ARGV[0] eq '--data' && shift(@ARGV);

    # KV args
    @ARGV = grep {/=/ ? (($data{$`}=$') && 0) : 1} @ARGV;

    my $template;
    if (-e $ARGV[0]) {
      $template = join("",<>);
      }
    else {
      $template = join("",<DATA>);
      }

    use_test_data(\%data) if (!%data);

    print main::render($template, \%data);
    }
main() if $0 eq __FILE__;
1;

__DATA__
Test Template
Interpolates only "names" following @ and $, these don't work: @123 @123dog @["dog"] $"dog"
Iterates a block: \@something[ stuff repeated ]
Within an iteration, also iterpolates \$_ and \$0..\$9 (see below)
Must otherwise balance left and right [] in iterations so the \@xxx[...] works

Literal @ and literal $ (because no "word" after sigil)
Escape dollar-sign: \$name
Escape at-sign \@outer[] \@outer[ x ]
Escape un-balanced brackets \] \[ (especially inside iterations)
Catenate interpolation: pretend scalar is array: Prefix@name[$_]Suffix
Can't escape backslash \\, but \\@outer[] is \ followed by \@..., thus dropping one \
No re-interpolation: $has_dollar_in_value

Top level scalar interpolation of \$file: $file
Top level name: $name

Iterate, as if foreach (@{\$data->{'outer'}}) {render(..)} : @outer[
  Level1 $name
    Also, \$_ is set to the each: $_
    Unbalanced \] and \[ must be escaped, but [ balanced ] ones are fine
    Top-level value visible: file=$file
    Level1 name overrides top-level: $name
    Level1 outerValue: $outerValue
    Starting inner list: @inner[
    Inner $name
      Level2 name overrides level1: $name
      Top-level values still visible: file=$file
      Level1 value visible: outerValue=$outerValue
    end-of-inner]
  end-of-Level1
  ]

If a list holds lists, you can index the inner with \$0..\$9
List of lists @lol[
  Elements of array $_
    \$0 = $0
    \$2 = $2 
    \$0..\$9 (and \$_) does catenate within an iteration, but that's deprecated: $0Catenated $_Catenated
    balanced brackets cause no problems: [ x ]. Nor escaped ones: \] 
    And, you can simply get the elements of a list: @_[$_ ]]
    (Note the "\$no_recursive_interpolation_of_data" value above)
end lol

Terminal $name text
