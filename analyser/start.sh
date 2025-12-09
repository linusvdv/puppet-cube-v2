python3 -m venv .venv
source .venv/bin/activate
pip install numpy

#
# cat ../out/out_*_many | grep "'" | sed 's/.*EXTRA: //' | sed -r 's/\x1b\[[0-9;]*m//g'
