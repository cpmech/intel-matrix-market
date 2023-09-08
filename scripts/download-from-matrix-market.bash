#!/bin/bash

set -e

mkdir -p ~/Downloads/matrix-market
cd ~/Downloads
cd matrix-market

get_matrix() {
  GROUP=$1
  NAME=$2
  wget https://suitesparse-collection-website.herokuapp.com/MM/$GROUP/$NAME.tar.gz
  tar xzf $NAME.tar.gz
  mv $NAME/$NAME.mtx .
  rm -rf $NAME
  rm $NAME.tar.gz
}

get_matrix Bai bfwb62
#get_matrix GHS_psdef inline_1
#get_matrix CEMW tmt_unsym
get_matrix ATandT pre2
#get_matrix ATandT twotone
#get_matrix Janna Flan_1565
#get_matrix Vavasis av41092
#get_matrix GHS_indef helm2d03
#get_matrix GHS_psdef oilpan
