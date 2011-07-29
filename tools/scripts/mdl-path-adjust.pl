#!/usr/bin/perl

# 
# 2011-07-29, Fi
# 
# adjust block path conventions between different EtherLAB versions
#
# $Date$
# $Revision$
# $Id$
#

use Getopt::Std; 

my %opts = (
            o => "1.2", 
            n => "1.3", 
            h => 0,
);

sub help {
    print "\n",
    "  adjust block path conventions between different EtherLAB versions\n\n",
    "     $0 [-o old_version] [-n new_version] [mdl ...]\n\n",
    "  each model will be saved as bak-file\n\n",
    "  conversions implemented:\n",
    "    $opts{'o'} -> $opts{'n'}\n\n";
    exit 0;         
}

getopts("o:n:h", \%opts);
my $old = $opts{'o'};
my $new = $opts{'n'};

help if $opts{"h"};

my $f;
for $f (@ARGV) {
    my $bak = "$f.$old"; 
    rename $f, $bak or do {
        warn "cannot create backup file $bak";
        next;
    };
    open F, $bak or do {
        warn "cannot read file $bak";
        next; 
    };
    open M, ">$f"  or do {
        warn "cannot open $f for writing";
        next; 
    };
    while ( <F> ) {        
        if ( $old eq "1.2" && $new eq "1.3" ) {
            # assume at most one entry a line
            goto OUT if m+etherlab_lib/EtherCAT/Generic Slave+;
            goto OUT if m+etherlab_lib/EtherCAT/Master State+; 
            goto OUT if m+etherlab_lib/EtherCAT/Domain State+; 
            s+etherlab_lib/World Time+etherlab_lib/Util/World Time+;
            s+etherlab_lib/Event+etherlab_lib/Util/Event+;
            s+etherlab_lib/EtherCAT/+etherlab_lib/EtherCAT/Beckhoff/+;
          OUT: print M; 
        } else {
            die "conversion $old to $new not available";                
        }
    }
}






            
