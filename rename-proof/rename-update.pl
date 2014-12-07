#!/usr/bin/perl

use strict;

# Popluate W and R based on a path $me and S($me)
sub S_to_wr {
   my $S = shift; # S($me)
   my $me = shift; # a path $me
   my $WR = shift;
   return if $S eq '0' or $S eq $me;
   $WR->{'w'}->{$me} = $S;
   $WR->{'r'}->{$S} = $me;
   return;
}

sub print_WR {
   my $WR = shift;
   return join(' ', (
     join(' ',sort map { 'w('.$_.')='.$WR->{'w'}->{$_} } keys %{$WR->{'w'}} ),
     join(' ',sort map { 'r('.$_.')='.$WR->{'r'}->{$_} } keys %{$WR->{'r'}} )
   ));
}

sub transform_WR {
   my $WR = shift;
   my $w = $WR->{'w'};
   my $r = $WR->{'r'};

   unless($w->{'a'}){
      $w->{'b'} = 'a';
   }
   else{
      $w->{'b'} = $w->{'a'};
   }

}

# Loop through all possible values of S(a) and S(b)
# 0 = \bot
foreach my $Sa (qw( 0 a b x )){
   foreach my $Sb (qw( 0 a b x y )){
      # Filter out invalid arrangements
      # We know that S is injective
      next if $Sa ne '0' and $Sb ne '0' and $Sa eq $Sb;

      # Filter out unneeded arrangements
      # y is only needed if x is used
      next if $Sb eq 'y' and $Sa ne 'x';

      # The rename operation in terms of S
      my $Sb2 = $Sa;
      my $Sa2 = '0';

      # Calculate old and new W and R
      my $WRold = {'w'=>{}, 'r'=>{}};
      S_to_wr($Sa, 'a', $WRold);
      S_to_wr($Sb, 'b', $WRold);

      my $WRnew = {'w'=>{}, 'r'=>{}};
      S_to_wr($Sa2, 'a', $WRnew);
      S_to_wr($Sb2, 'b', $WRnew);

      print "$Sa $Sb -> $Sa2 $Sb2 == ".print_WR($WRold).' -> '.print_WR($WRnew)."\n";

      # Now transform the old W,R using the proposed translation
      transform_WR($WRold);

      # Check if we get $WRnew
      foreach my $wr (qw( w r )){
         foreach my $sk (keys %{$WRnew->{$wr}}){
            if($WRnew->{$wr}->{$sk} ne $WRold->{$wr}->{$sk}){
#               print "ERROR at $wr $sk: S-based transfrom resulted in \'$WRnew->{$wr}->{$sk}\' while WR-based transform resulted in \'$WRold->{$wr}->{$sk}\'\n";
            }
            delete $WRold->{$wr}->{$sk};
         }
         foreach my $wrk (keys %{$WRold->{$wr}}){
#            print "ERROR at $wr $wrk: S-based transfrom resulted in no mark while WR-based transform resulted in \'$WRold->{$wr}->{$wrk}\'\n";
         }
      }
   }
}
